#include "redis.h"
#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
#include <dlfcn.h>

// [CONNECTOR-NAME]://(!)[CONNECTOR-COMMAND]
#define HEDIS_PROTOCOL_PATTERN "(\\w+)(://!?)(.+)"
#define HEDIS_PROTOCOL_NAME_INDEX 0
#define HEDIS_PROTOCOL_INVALIDATE_INDEX 1
#define HEDIS_PROTOCOL_COMMAND_INDEX 2
#define MAX_ERROR_MSG 0x1000
#define BUILD_COMMAND "./build_connector.sh %s"

hedisConnectorList hedis_connector_list;

void print_hedis_connector() {
    for (int i = 0; i < hedis_connector_list.connector_count; i++) {
        printf("hedis_connector_list.connectors[%d]->name: %s\n", i, hedis_connector_list.connectors[i]->name);

        for (int j = 0; j < hedis_connector_list.connectors[i]->entry_count; j++) {
            printf("key: %s, value: %s\n", hedis_connector_list.connectors[i]->entries[j]->key, hedis_connector_list.connectors[i]->entries[j]->value);
        }
    }
}

char *get_hedis_value(char **str) {
    for (int i = 0; i < hedis_connector_list.connector_count; i++) {
        if (!strcasecmp(hedis_connector_list.connectors[i]->name, str[HEDIS_PROTOCOL_NAME_INDEX])) {
            void *lib = hedis_connector_list.connectors[i]->lib;

            // load library fail
            if (lib == NULL) {
                return NULL;
            }

            char *(*get_value)(const char *) = dlsym(lib, "get_value");

            return (*get_value)(str[HEDIS_PROTOCOL_COMMAND_INDEX]);
        }
    }

    return NULL;
}

int build_connector(hedisConnector *connector) {
    char *command;

    command = malloc(sizeof(char) * (strlen(BUILD_COMMAND) + strlen(connector->type) * 1 + 5));

    sprintf(command, BUILD_COMMAND, connector->type);

    redisLog(REDIS_WARNING, "Loading %s ...", connector->type);

    system(command);
}

void load_connector(hedisConnector *connector) {
    char *lib_name;

    lib_name = malloc(sizeof(char) * 40);

    sprintf(lib_name, "libhedis-connector-%s.so", connector->type);

    void *lib = dlopen(lib_name, RTLD_LAZY);

    if (!lib) {
        redisLog(REDIS_WARNING, "Load %s error, %s", lib_name, dlerror());

        connector->lib = NULL;

        return;
    }

    dlerror();

    for (int i = 0; i < connector->env_entry_count; i++) {
        hedisConfigEntry *envEntry = connector->env_entries[i];

        redisLog(REDIS_WARNING, "export %s=%s", envEntry->key, envEntry->value);

        // nonzero value will overwrite
        setenv(envEntry->key, envEntry->value, 1);
    }

    int (*init)(hedisConfigEntry **, int) = dlsym(lib, "init");

    int status = (*init)(connector->entries, connector->entry_count);

    if (status != 0) {
        redisLog(REDIS_WARNING, "Initialize %s error", connector->name);

        connector->lib = NULL;

        return;
    }

    connector->lib = lib;

    free(lib_name);
}

void load_hedis_connectors() {
    for (int i = 0; i < hedis_connector_list.connector_count; i++) {
        build_connector(hedis_connector_list.connectors[i]);

        load_connector(hedis_connector_list.connectors[i]);
    }
}

sds join_string(const char **s, int split_len, const char *separator) {
    sds join_str;

    if (split_len > 2) {
        join_str = sdscat(s[1], separator);

        for (int i = 2; i < split_len; i++) {
            join_str = sdscat(join_str, s[i]);
            join_str = sdscat(join_str, separator);
        }

        join_str = sdstrim(join_str, " \t\r\n");
    } else {
        join_str = sdstrim(s[1], " \t\r\n");
    }

    return sdstrim(join_str, separator);
}

sds **parse_yaml_entry(int *index, sds *lines, int *type, int *entry_count) {
    if (lines[*index][0] != ' ') {
        // name entry
        sds **kv = zmalloc(sizeof(sds *) * 1);

        kv[0] = zmalloc(sizeof(sds) * 2);

        kv[0][0] = zmalloc(sizeof(sds) * (strlen(lines[*index]) + 1));
        kv[0][1] = NULL;

        strcpy(kv[0][0], sdstrim(lines[*index], ":\n"));

        *type = 0;
        *entry_count = 1;

        return kv;
    } else {
        int split_len = -1;

        lines[*index] = sdstrim(lines[*index], "\t\r\n");

        sds **kv = zmalloc(sizeof(sds *) * 1);

        kv[0] = sdssplitlen(lines[*index], strlen(lines[*index]), ":", 1, &split_len);

        kv[0][0] = sdstrim(kv[0][0], "\t\r\n");
        kv[0][1] = join_string(kv[0], split_len, ":");

        if (!strcasecmp(kv[0][0], "  type")) {
            // type entry
            kv[0][0] = sdstrim(kv[0][0], " ");
            kv[0][1] = sdstrim(kv[0][1], " ");

            *type = 1;

            return kv;
        } else if (!strcasecmp(kv[0][0], "  env")) {
            // env entry
            sds **env_kv = zmalloc(sizeof(sds *) * 10);

            int env_entry_index = -1;

            while (1) {
                (*index)++;
                env_entry_index++;

                if (lines[*index] == NULL) {
                    *entry_count = env_entry_index;
                    *type = 3;

                    return env_kv;
                }

                lines[*index] = sdstrim(lines[*index], "\t\r\n");

                env_kv[env_entry_index] = sdssplitlen(lines[*index], strlen(lines[*index]), ":", 1, &split_len);

                if (!strncmp(env_kv[env_entry_index][0], "    ", 4)) {
                    env_kv[env_entry_index][0] = sdstrim(env_kv[env_entry_index][0], " ");
                    env_kv[env_entry_index][1] = join_string(env_kv[env_entry_index], split_len, ":");
                } else {
                    (*index)--;
                    env_entry_index--;

                    break;
                }
            }

            *entry_count = env_entry_index + 1;
            *type = 3;

            return env_kv;
        } else {
            // common entry
            sds **common_kv = zmalloc(sizeof(sds *) * 10);

            int entry_index = 0;

            kv[0][0] = sdstrim(kv[0][0], " ");
            kv[0][1] = sdstrim(kv[0][1], " ");

            common_kv[0] = kv[0];

            while (1) {
                (*index)++;
                entry_index++;

                lines[*index] = sdstrim(lines[*index], "\t\r\n");

                if (!strcasecmp(lines[*index], "  env:")) {
                    (*index)--;
                    *entry_count = entry_index;
                    *type = 2;

                    return common_kv;
                }

                common_kv[entry_index] = sdssplitlen(lines[*index], strlen(lines[*index]), ":", 1, &split_len);

                if (!strncmp(common_kv[entry_index][0], "  ", 2)) {
                    common_kv[entry_index][0] = sdstrim(common_kv[entry_index][0], " ");
                    common_kv[entry_index][1] = join_string(common_kv[entry_index], split_len, ":");
                } else {
                    (*index)--;
                    entry_index--;

                    break;
                }
            }

            *entry_count = entry_index + 1;
            *type = 2;

            return common_kv;
        }
    }

    return NULL;
}

int parse_hedis_config(const char * filename) {
    sds all_config = sdsempty();
    char buf[REDIS_CONFIGLINE_MAX + 1];
    FILE *file = fopen(filename, "r");

    if (!file) {
        redisLog(REDIS_WARNING, "Fatal error, can't open Hedis config file '%s'", filename);

        exit(1);
    }

    while(fgets(buf, REDIS_CONFIGLINE_MAX + 1, file) != NULL) {
        // TODO: ignore comments

        all_config = sdscat(all_config, buf);

        sds config = sdsnew(buf);

        // New connector
        if (config[0] != ' ') {
            hedis_connector_list.connector_count++;
        }
    }

    fclose(file);

    hedis_connector_list.connectors = zmalloc(sizeof(hedisConnector *) * hedis_connector_list.connector_count);

    int line_count = -1;
    int connector_index = -1;
    int env_entry_index = -1;
    sds *lines = sdssplitlen(all_config, strlen(all_config), "\n", 1, &line_count);

    hedisConnector *connector;

    int i = 0;

    while (i <= line_count) {
        int type = -1;
        int entry_count = -1;

        sds **kv = parse_yaml_entry(&i, lines, &type, &entry_count);

        if (kv == NULL) {
            redisLog(REDIS_WARNING, "Parse error, can't parse '%s'", lines[i]);

            exit(1);
        }

        switch (type) {
            // name entry
            case 0:
                connector_index++;

                hedis_connector_list.connectors[connector_index] = zmalloc(sizeof(hedisConnector));

                connector = hedis_connector_list.connectors[connector_index];

                connector->name = zmalloc(sizeof(char) * (strlen(kv[0][0]) + 1));

                strcpy(connector->name, kv[0][0]);

                break;
            // type entry
            case 1:
                connector->type = zmalloc(sizeof(char) * (strlen(kv[0][1]) + 1));

                strcpy(connector->type, kv[0][1]);

                break;
            // common entry
            case 2:
                connector->entries = zmalloc(sizeof(hedisConfigEntry *) * entry_count);
                connector->entry_count = entry_count;

                for (int j = 0; j < entry_count; j++) {
                    connector->entries[j] = zmalloc(sizeof(hedisConfigEntry));
                    connector->entries[j]->key = zmalloc(sizeof(char) * (strlen(kv[j][0]) + 1));
                    connector->entries[j]->value = zmalloc(sizeof(char) * (strlen(kv[j][1]) + 1));

                    strcpy(connector->entries[j]->key, kv[j][0]);
                    strcpy(connector->entries[j]->value, kv[j][1]);
                }

                break;
            // env entry
            case 3:
                connector->env_entries = zmalloc(sizeof(hedisConfigEntry *) * entry_count);
                connector->env_entry_count = entry_count;

                for (int j = 0; j < entry_count; j++) {
                    connector->env_entries[j] = zmalloc(sizeof(hedisConfigEntry));
                    connector->env_entries[j]->key = zmalloc(sizeof(char) * (strlen(kv[j][0]) + 1));
                    connector->env_entries[j]->value = zmalloc(sizeof(char) * (strlen(kv[j][1]) + 1));

                    strcpy(connector->env_entries[j]->key, kv[j][0]);
                    strcpy(connector->env_entries[j]->value, kv[j][1]);
                }

                break;
        }

        i++;
    }

    return 0;
}

hedisProtocol *parse_hedis_protocol(const char * to_match) {
    regex_t * r = malloc(sizeof(regex_t));

    int status = regcomp(r, HEDIS_PROTOCOL_PATTERN, REG_EXTENDED | REG_NEWLINE);

    if (status != 0) {
        char error_message[MAX_ERROR_MSG];

        regerror(status, r, error_message, MAX_ERROR_MSG);

        printf("Regex error compiling '%s': %s\n", HEDIS_PROTOCOL_PATTERN, error_message);

        return NULL;
    }

    char **str = malloc(sizeof(char *) * 3);

    /* "P" is a pointer into the string which points to the end of the
     *        previous match. */
    const char * p = to_match;
    /* "N_matches" is the maximum number of matches allowed. */
    const int n_matches = 10;
    /* "M" contains the matches found. */
    regmatch_t m[n_matches];

    int i = 0;
    int nomatch = regexec(r, p, n_matches, m, 0);

    if (nomatch) {
        printf("No more matches.\n");

        return NULL;
    }

    for (i = 0; i < n_matches; i++) {
        int start;
        int finish;

        if (m[i].rm_so == -1) {
            break;
        }

        start = m[i].rm_so + (p - to_match);
        finish = m[i].rm_eo + (p - to_match);

        if (i != 0) {
            int size = finish - start;

            str[i - 1] = malloc(sizeof(char) * (size + 1));

            sprintf(str[i - 1], "%.*s", size, to_match + start);
        }
    }

    p += m[0].rm_eo;

    hedisProtocol *protocol = malloc(sizeof(hedisProtocol));

    if (!strcasecmp(str[HEDIS_PROTOCOL_INVALIDATE_INDEX], "://")) {
        protocol->invalidate = HEDIS_INVALIDATE_PRESERVE;
    } else if (!strcasecmp(str[HEDIS_PROTOCOL_INVALIDATE_INDEX], "://!")) {
        protocol->invalidate = HEDIS_INVALIDATE_MUTATION;
    }

    protocol->command = str;

    regfree(r);
    free(r);

    return protocol;
}