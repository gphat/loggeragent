#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

struct handler {
	char	*name;
	void	(*func)();
};

struct module {
	char	*name;
	char	*version;
	void 	*dl;
	void	(*destroyfunc)();
};

void load_modules(void);

void load_module(char *);

void module_stats(void);

int has_handler(char *);

void call_handler(char *, xmlDocPtr, xmlNodePtr);

void unload_modules(void);
