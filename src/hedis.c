#include "redis.h"
#include <yaml.h>
#include <dlfcn.h>

hedisConnectorList *hedis_connector_list;

char *get_hedis_value(const char ** str) {
    return str[1];
}

void *load_connector(const char * connector_name, const int connector_index) {
    char *lib_name;

    lib_name = malloc(sizeof(char) * 30);

    sprintf(lib_name, "libhedis-connector-%s.so", connector_name);

    void *lib = dlopen(lib_name, RTLD_LAZY);

    if (!lib) {
        redisLog(REDIS_WARNING, "Load %s error, %s", lib_name, dlerror());

        return NULL;
    }

    dlerror();

    int (*init)(hedisConfigEntry *) = dlsym(lib, "init");

    hedisConfigEntry *entry = malloc(sizeof(hedisConfigEntry));

    entry->key = malloc(sizeof(char) * 20);
    entry->value = malloc(sizeof(char) * 20);

    strcpy(entry->key, "abc");
    strcpy(entry->value, "def");

    int status = (*init)(entry);

    if (status != 0) {
        redisLog(REDIS_WARNING, "Initialize %s error", connector_name);

        return NULL;
    }

    return lib;
}

int parse_hedis_config(const char * filename) {
    FILE *file = fopen(filename, "r");

    if (!file) {
        redisLog(REDIS_WARNING, "Load Hedis configuration error");

        return -1;
    }

    yaml_parser_t parser;
    yaml_token_t token;

    int connector_count = count_connectors(file);

    hedis_connector_list = malloc(sizeof(hedisConnectorList *));

    hedis_connector_list->connector_count = connector_count;
    hedis_connector_list->connectors = malloc(sizeof(hedisConnector *) * connector_count);

    for (int i = 0; i < connector_count; i++) {
        hedis_connector_list->connectors[i] = malloc(sizeof(hedisConnector));

        hedis_connector_list->connectors[i]->name = malloc(sizeof(char) * 30);
        hedis_connector_list->connectors[i]->entries = malloc(sizeof(hedisConfigEntry) * 10);
    }

    if (!yaml_parser_initialize(&parser)) {
        fputs("Failed to initialize parser!\n", stderr);
    }

    yaml_parser_set_input_file(&parser, file);

    int token_type = -1;
    int connector_index = -1;
    hedisConfigEntry *entry;
    int entry_index = -1;

    do {
        yaml_parser_scan(&parser, &token);

        char *value;

        switch (token.type) {
        /* Stream start/end */
        case YAML_STREAM_START_TOKEN:
            puts("STREAM START");

            break;
        case YAML_STREAM_END_TOKEN:
            puts("STREAM END");

            break;
        /* Token types (read before actual token) */
        case YAML_KEY_TOKEN:
            printf("(Key token)   ");

            if (parser.indent != 0) {
                entry = malloc(sizeof(hedisConfigEntry));

                entry->key = malloc(sizeof(char) * 30);
                entry->value = malloc(sizeof(char) * 30);

                token_type = 0;
            }

            break;
        case YAML_VALUE_TOKEN:
            printf("(Value token) ");

            token_type = 1;

            break;
        /* Block delimeters */
        case YAML_BLOCK_SEQUENCE_START_TOKEN:
            puts("<b>Start Block (Sequence)</b>");

            break;
        case YAML_BLOCK_ENTRY_TOKEN:
            puts("<b>Start Block (Entry)</b>");

            break;
        case YAML_BLOCK_END_TOKEN:
            puts("<b>End block</b>");

            break;
        /* Data */
        case YAML_BLOCK_MAPPING_START_TOKEN:
            puts("[Block mapping]");

            break;
        case YAML_SCALAR_TOKEN:
            value = token.data.scalar.value;

            if (parser.indent == 0) {
                connector_index++;

                entry_index = -1;

                strcpy(hedis_connector_list->connectors[connector_index]->name, value);
            } else {
                if (token_type == 0) {
                    strcpy(entry->key, value);
                } else if (token_type == 1) {
                    strcpy(entry->value, value);

                    entry_index++;

                    hedis_connector_list->connectors[connector_index]->entries[entry_index] = entry;
                    hedis_connector_list->connectors[connector_index]->entry_count = entry_index + 1;
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

    for (int i = 0; i < hedis_connector_list->connector_count; i++) {
        printf("hedis_connector_list->connectors[%d]->name: %s\n", i, hedis_connector_list->connectors[i]->name);

        for(int j = 0; j < hedis_connector_list->connectors[i]->entry_count; j++){
            printf("key: %s, value: %s\n", hedis_connector_list->connectors[i]->entries[j]->key, hedis_connector_list->connectors[i]->entries[j]->value);
        }
    }

    return 0;
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

void load_hedis_connectors(){
    printf("load_hedis_connectors\n");
}