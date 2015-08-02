#include "redis.h"
#include <stdlib.h>
#include <stdio.h>
#include <regex.h>
#include <yaml.h>
#include <dlfcn.h>

// [CONNECTOR-NAME]://(!)[CONNECTOR-COMMAND]
#define HEDIS_PROTOCOL_PATTERN "(\\w+)(://!?)(.+)"
#define HEDIS_PROTOCOL_NAME_INDEX 0
#define HEDIS_PROTOCOL_INVALIDATE_INDEX 1
#define HEDIS_PROTOCOL_COMMAND_INDEX 2
#define MAX_ERROR_MSG 0x1000

hedisConnectorList hedis_connector_list;

void print_hedis_connector(){
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
}

void load_hedis_connectors() {
    for (int i = 0; i < hedis_connector_list.connector_count; i++) {
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

int parse_hedis_config(const char * filename) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        redisLog(REDIS_WARNING, "Load Hedis configuration error");

        return -1;
    }

    yaml_parser_t parser;
    yaml_token_t token;

    size_t connector_count = count_connectors(file);

    hedis_connector_list.connector_count = connector_count;
    hedis_connector_list.connectors = malloc(sizeof(hedisConnector *) * connector_count);

    for (size_t i = 0; i < connector_count; i++) {
        hedis_connector_list.connectors[i] = malloc(sizeof(hedisConnector));

        hedis_connector_list.connectors[i]->entries = malloc(sizeof(hedisConfigEntry) * 10);
        hedis_connector_list.connectors[i]->env_entries = malloc(sizeof(hedisConfigEntry) * 10);
    }

    if (!yaml_parser_initialize(&parser)) {
        fputs("Failed to initialize parser!\n", stderr);
    }

    yaml_parser_set_input_file(&parser, file);

    yaml_token_type_t token_type = YAML_NO_TOKEN;
    int connector_index = -1;
    int entry_index = -1;
    int env_entry_index = -1;
    int env_type = -1;
    hedisConfigEntry *entry = NULL;

    do {
        yaml_parser_scan(&parser, &token);

        yaml_char_t *value;
        size_t value_len;

        switch (token.type) {
        /* Stream start/end */
        case YAML_STREAM_START_TOKEN:
            // puts("STREAM START");

            break;
        case YAML_STREAM_END_TOKEN:
            // puts("STREAM END");

            break;
        /* Token types (read before actual token) */
        case YAML_KEY_TOKEN:
            // printf("(Key token)   ");

            if (parser.indent != 0) {
                entry = malloc(sizeof(hedisConfigEntry));

                token_type = YAML_KEY_TOKEN;
            }

            break;
        case YAML_VALUE_TOKEN:
            // printf("(Value token) ");

            token_type = YAML_VALUE_TOKEN;

            break;
        /* Block delimeters */
        case YAML_BLOCK_SEQUENCE_START_TOKEN:
            // puts("<b>Start Block (Sequence)</b>");

            break;
        case YAML_BLOCK_ENTRY_TOKEN:
            // puts("<b>Start Block (Entry)</b>");

            break;
        case YAML_BLOCK_END_TOKEN:
            // puts("<b>End block</b>");

            break;
        /* Data */
        case YAML_BLOCK_MAPPING_START_TOKEN:
            // puts("[Block mapping]");

            break;
        case YAML_SCALAR_TOKEN:
            value = token.data.scalar.value;
            value_len = token.data.scalar.length;

            if (parser.indent == 0) {
                connector_index++;

                entry_index = -1;
                env_entry_index = -1;
                env_type = -1;

                hedis_connector_list.connectors[connector_index]->name = malloc(sizeof(char) * (value_len + 1));

                strcpy(hedis_connector_list.connectors[connector_index]->name, (const char *)value);
            } else {
                if (token_type == YAML_KEY_TOKEN) {
                    if (strcasecmp((const char *)value, "env") || env_type == 1) {
                        entry->key = malloc(sizeof(char) * (value_len + 1));

                        strcpy(entry->key, (const char *)value);
                    } else {
                        env_type = 1;
                    }
                } else if (token_type == YAML_VALUE_TOKEN) {
                    if (!strcasecmp(entry->key, "type")) {
                        hedis_connector_list.connectors[connector_index]->type = malloc(sizeof(char) * (value_len + 1));

                        strcpy(hedis_connector_list.connectors[connector_index]->type, (const char *)value);
                    }

                    entry->value = malloc(sizeof(char) * (value_len + 1));

                    strcpy(entry->value, (const char *)value);

                    if (env_type == 1) {
                        env_entry_index++;

                        hedis_connector_list.connectors[connector_index]->env_entries[env_entry_index] = entry;
                        hedis_connector_list.connectors[connector_index]->env_entry_count = env_entry_index + 1;
                    } else {
                        entry_index++;

                        hedis_connector_list.connectors[connector_index]->entries[entry_index] = entry;
                        hedis_connector_list.connectors[connector_index]->entry_count = entry_index + 1;
                    }
                }
            }

            break;
        /* Others */
        default:
            printf("Got token of type %d\n", token.type);
        }

        if (token.type != YAML_STREAM_END_TOKEN) {
            yaml_token_delete(&token);
        }
    } while (token.type != YAML_STREAM_END_TOKEN);

    yaml_token_delete(&token);
    /* END new code */

    /* Cleanup */
    yaml_parser_delete(&parser);

    fclose(file);

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

            str[i - 1] = malloc(sizeof(char) * size);

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

    return protocol;
}