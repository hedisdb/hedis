typedef struct {
	char *key;
	char *value;
} hedisConfigEntry;

typedef struct {
	char *name;
	int entry_count;
	hedisConfigEntry **entries;
	void *lib;
} hedisConnector;

typedef struct {
	int connector_count;
	hedisConnector **connectors;
} hedisConnectorList;