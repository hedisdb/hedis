/* HBase Config */
typedef struct {
    char *name;
    char *zookeepers;
} hbaseConfig;

typedef struct {
	hbaseConfig **hbase_configs;
	int hbase_config_count;
} hedisConfig;

typedef enum {
	HEDIS_TYPE_UNDEFINED,
	HEDIS_TYPE_HBASE,
	HEDIS_TYPE_MYSQL
} hedisType;

typedef struct {
	char *type;
	void *lib;
} hedisConnector;

typedef struct {
	int connector_count;
	hedisConnector **hedisConnector;
} hedisConnectorList;