#include "redis.h"

hedisType get_hedis_type(const char * db_name) {
    for (int i = 0; i < hedis_config->hbase_config_count; i++) {
        if (!strcasecmp(hedis_config->hbase_configs[i]->name, db_name)) {
			return HEDIS_TYPE_HBASE;
        }
    }

    return HEDIS_TYPE_UNDEFINED;
}

char *get_hedis_value(const char ** str) {
    hedisType type = get_hedis_type(str[0]);

	if (type == HEDIS_TYPE_UNDEFINED) {
		return NULL;
	}

	return str[1];
}