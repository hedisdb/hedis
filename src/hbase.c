#include "redis.h"
#include <regex.h>
#include <yaml.h>

// dbname://tablename:rowkey
#define HBASE_COMMAND_PATTERN "(\\w+)://(\\w+):(\\w+)"
#define MAX_ERROR_MSG 0x1000

char **parse_hbase_protocol(const char * to_match){
  regex_t * r = malloc(sizeof(regex_t));

  int status = regcomp(r, HBASE_COMMAND_PATTERN, REG_EXTENDED|REG_NEWLINE);

  if (status != 0) {
    char error_message[MAX_ERROR_MSG];

    regerror(status, r, error_message, MAX_ERROR_MSG);

    printf("Regex error compiling '%s': %s\n", HBASE_COMMAND_PATTERN, error_message);

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

  return str;
}

hedisConfig *parse_hedis_config(const char * filename){
  FILE *file = fopen(filename, "r");

  yaml_parser_t parser;
  yaml_token_t token;

  int config_counts = count_entries(file);

  hedisConfig *config = malloc(sizeof(hedisConfig));

  config->hbase_config_count = config_counts;
  config->hbase_configs = malloc(sizeof(hbaseConfig *) * config_counts);

  for (int i = 0; i < config_counts; i++) {
    config->hbase_configs[i] = malloc(sizeof(hbaseConfig));
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

    switch(token.type) {
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

      if (current_index != -1){
        token_type = 0;
      }

      break;
    case YAML_VALUE_TOKEN:
      printf("(Value token) ");

      if (current_index != -1){
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

      if (token_type == 0) {
        if (!strcasecmp(value, "name")) {
          value_type = 0;
        } else if (!strcasecmp(value, "zookeepers")) {
          value_type = 1;
        }
      } else if (token_type == 1){
        if (value_type == 0) {
          config->hbase_configs[current_index]->name = malloc(sizeof(char) * strlen(value));

          strcpy(config->hbase_configs[current_index]->name, value);
        } else if (value_type == 1) {
          config->hbase_configs[current_index]->zookeepers = malloc(sizeof(char) * strlen(value));

          strcpy(config->hbase_configs[current_index]->zookeepers, value);
        }
      }

      break;
    /* Others */
    default:
      printf("Got token of type %d\n", token.type);
    }

    if(token.type != YAML_STREAM_END_TOKEN) {
      yaml_token_delete(&token);
    }
  } while(token.type != YAML_STREAM_END_TOKEN);

  yaml_token_delete(&token);
  /* END new code */

  /* Cleanup */
  yaml_parser_delete(&parser);

  fclose(file);

  return config;
}

int count_entries(FILE *file) {
  yaml_parser_t parser;
  yaml_token_t token;
  int counts = 0;

  /* Initialize parser */
  if(!yaml_parser_initialize(&parser)) {
      fputs("Failed to initialize parser!\n", stderr);
  }

  if(file == NULL) {
      fputs("Failed to open file!\n", stderr);
  }

  /* Set input file */
  yaml_parser_set_input_file(&parser, file);

  /* BEGIN new code */
  // calculate entry counts
  do {
    yaml_parser_scan(&parser, &token);

    if (token.type == YAML_BLOCK_ENTRY_TOKEN){
      counts++;
    }

    if(token.type != YAML_STREAM_END_TOKEN) {
      yaml_token_delete(&token);
    }
  } while(token.type != YAML_STREAM_END_TOKEN);

  /* Cleanup */
  yaml_parser_delete(&parser);

  fseek(file, 0, SEEK_SET);

  return counts;
}