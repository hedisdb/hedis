typedef struct {
	char *name;
	void *lib;
} hedisConnector;

typedef struct {
	int connector_count;
	hedisConnector **connectors;
} hedisConnectorList;

typedef struct {
	char *key;
	char *value;
} hedisConfigEntry;