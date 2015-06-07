/* HBase Config */
typedef struct {
    char *name;
    char *zookeepers;
} hbaseConfig;

typedef struct {
	char *name;
	void *lib;
} hedisConnector;

typedef struct {
	int connector_count;
	hedisConnector **connectors;
} hedisConnectorList;