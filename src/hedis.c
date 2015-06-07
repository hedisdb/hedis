#include "redis.h"
#include <yaml.h>

hedisConnectorList *hedis_connector_list;

char *get_hedis_value(const char ** str) {
    return str[1];
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
    }

    if (!yaml_parser_initialize(&parser)) {
        fputs("Failed to initialize parser!\n", stderr);
    }

    yaml_parser_set_input_file(&parser, file);

    int current_index = -1;
    int next_index = 0;
    int token_type = -1;
    int value_type = -1;

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

            if (current_index != -1) {
                token_type = 0;
            }

            break;
        case YAML_VALUE_TOKEN:
            printf("(Value token) ");

            if (current_index != -1) {
                token_type = 1;
            }

            break;
        /* Block delimeters */
        case YAML_BLOCK_SEQUENCE_START_TOKEN:
            puts("<b>Start Block (Sequence)</b>");

            break;
        case YAML_BLOCK_ENTRY_TOKEN:
            puts("<b>Start Block (Entry)</b>");

            current_index = next_index;

            next_index++;

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
                load_connector(value);
            }

            // if (token_type == 0) {
            //   if (!strcasecmp(value, "name")) {
            //     value_type = 0;
            //   } else if (!strcasecmp(value, "zookeepers")) {
            //     value_type = 1;
            //   }
            // } else if (token_type == 1){
            //   if (value_type == 0) {
            //     hedis_config->hbase_configs[current_index]->name = malloc(sizeof(char) * strlen(value));

            //     strcpy(hedis_config->hbase_configs[current_index]->name, value);
            //   } else if (value_type == 1) {
            //     hedis_config->hbase_configs[current_index]->zookeepers = malloc(sizeof(char) * strlen(value));

            //     strcpy(hedis_config->hbase_configs[current_index]->zookeepers, value);
            //   }
            // }

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

        if (token.type == YAML_BLOCK_ENTRY_TOKEN && parser.indent == 0) {
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

void load_connector(const char * connector_name) {
    char lib_name[30] = malloc(sizeof(char) * 30);

    sprintf(lib_name, "libhedis-connector-%s.so", connector_name);

    void *lib = dlopen(lib_name, RTLD_LAZY);

    if (!lib) {
        redisLog(REDIS_WARNING, "Load %s error", lib_name);

        return;
    }
}