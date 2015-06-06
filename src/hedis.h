/* HBase Config */
typedef struct {
    char *name;
    char *zookeepers;
} hbaseConfig;

typedef struct {
	char *type;
	void *lib;
} hedisConnector;

typedef struct {
	int connector_count;
	hedisConnector **hedisConnector;
} hedisConnectorList;