typedef struct {
	char *key;
	char *value;
} hedisConfigEntry;

typedef struct {
	char *name;
	char *type;
	int entry_count;
	hedisConfigEntry **entries;
	void *lib;
} hedisConnector;

typedef struct {
	int connector_count;
	hedisConnector **connectors;
} hedisConnectorList;