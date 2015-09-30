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

int count_connectors(FILE *file) {
    yaml_parser_t parser;
    yaml_token_t token;
    int counts = 0;

    /* Initialize parser */
    if (!yaml_parser_initialize(&parser)) {
        fputs("Failed to initialize parser!\n", stderr);
    }

    if (file == NULL) {
        fputs("Failed to open file!\n", stderr);
    }

    /* Set input file */
    yaml_parser_set_input_file(&parser, file);

    /* BEGIN new code */
    // calculate connector counts
    do {
        yaml_parser_scan(&parser, &token);

        if (token.type == YAML_KEY_TOKEN && parser.indent == 0) {
            counts++;
        }

        if (token.type != YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
        }
    } while (token.type != YAML_STREAM_END_TOKEN);

    /* Cleanup */
    yaml_parser_delete(&parser);

    fseek(file, 0, SEEK_SET);

    return counts;
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
        kv[0][1] = sdstrim(kv[0][1], "\t\r\n");

        printf("KV = %s, %s\n", kv[0][0], kv[0][1]);

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

                lines[*index] = sdstrim(lines[*index], "\t\r\n");

                env_kv[env_entry_index] = sdssplitlen(lines[*index], strlen(lines[*index]), ": ", 2, &split_len);

                if (!strncmp(env_kv[env_entry_index][0], "    ", 4)) {
                    env_kv[env_entry_index][0] = sdstrim(env_kv[env_entry_index][0], " ");
                    env_kv[env_entry_index][1] = sdstrim(env_kv[env_entry_index][1], " ");
                } else {
                    (*index)--;

                    break;
                }
            }

            *entry_count = env_entry_index + 1;
            *type = 3;

            return env_kv;
        } else {
            // common entry
            return NULL;
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

        // if (kv == NULL) {
        //     redisLog(REDIS_WARNING, "Parse error, can't parse '%s'", lines[i]);

        //     exit(1);
        // }

        printf("entry_count = %d\n", entry_count);

        for (int j = 0; j < entry_count; j++) {
            printf("k = %s\n", kv[j][0]);
            printf("v = %s\n", kv[j][1]);
        }

        printf("type = %d\n", type);

        switch (type) {
            // name
            case 0:
                connector_index++;

                hedis_connector_list.connectors[connector_index] = zmalloc(sizeof(hedisConnector));

                connector = hedis_connector_list.connectors[connector_index];

                connector->name = zmalloc(sizeof(char) * (strlen(kv[0][0]) + 1));
                connector->entries = zmalloc(sizeof(hedisConfigEntry) * 10);

                strcpy(connector->name, kv[0][0]);

                printf("NAME = %s\n", connector->name);

                break;
            // type
            case 1:
                connector->type = zmalloc(sizeof(char) * (strlen(kv[0][1]) + 1));

                strcpy(connector->type, kv[0][1]);

                printf("TYPE = %s\n", connector->type);

                break;
            // common entry
            case 2:
                break;
            // env entry
            case 3:
                connector->env_entries = zmalloc(sizeof(hedisConfigEntry) * entry_count);

                for (int j = 0; j < entry_count; j++) {
                    connector->env_entries[j]->key = zmalloc(sizeof(char) * (strlen(kv[j][0]) + 1));
                    connector->env_entries[j]->value = zmalloc(sizeof(char) * (strlen(kv[j][1]) + 1));

                    strcpy(connector->env_entries[j]->key, kv[j][0]);
                    strcpy(connector->env_entries[j]->value, kv[j][1]);

                    printf("env_k = %s\n", connector->env_entries[j]->key);
                    printf("env_v = %s\n", connector->env_entries[j]->value);
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