#include <sys/types.h>

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "config.h"
#include "log.h"
#include "module.h"

#ifdef HAVE_MACH_O_DYLD_H
#include <mach-o/dyld.h>
#endif

struct handler **handlers = NULL;
int handler_count = 0;
struct module **dllist = NULL;
int call_count = 0;

void load_modules() {
	DIR	*plugins = NULL;
	struct dirent *ent;
	char *needle = NULL;
	char *sofile = NULL;
	char *plugindir = PLUGIN_DIR;

	plugins = opendir(plugindir);
	if(plugins == NULL) {
		llog(LOG_ERR, "opendir() failed: %s", strerror(errno));
		return;
	}

	llog(LOG_DEBUG, "Reading plugin directory, %s.", plugindir);
	while((ent = readdir(plugins)) != NULL) {
		needle = strstr(ent->d_name, ".so");
		if((needle != NULL) && (strlen(needle) == 3)) {
			llog(LOG_DEBUG, "Found file '%s'.", ent->d_name);
			sofile = (char *) lprintf("%s/%s", plugindir, ent->d_name);
			if(sofile == NULL) {
				llog(LOG_ERR, "lprintf() for sofile failed.");
				closedir(plugins);
				return;
			}
			llog(LOG_DEBUG, "Sending '%s' to load_module().", sofile);
			load_module(sofile);
			free(sofile);
		}
	}

	closedir(plugins);
}

void load_module(char *modfile) {
	void 	*dl;
	char	**harray;
	char	*modname;
	char	*modver;
	struct handler *h;
	struct module *m;
	void	(*initfunc)();
	int		i, count = 0;

	llog(LOG_DEBUG, "---- load_module(%s) ----", modfile);
	dl = dlopen(modfile, RTLD_NOW);
	if(dl == NULL) {
		llog(LOG_ERR, "Failed to load module %s: %s", modfile, dlerror());
		return;
	}

	modname 	= dlsym(dl, "modname");
	modver		= dlsym(dl, "modver");
	initfunc	= dlsym(dl, "init");
	if(!modname || !modver || !initfunc) {
		llog(LOG_ERR, "Failed to load module %s: could not identify module", modfile);
		dlclose(dl);
		return;
	}
	llog(LOG_INFO, "Loading module %s, version %s", modname, modver);

	harray = dlsym(dl, "exports");
	if(!harray) {
		llog(LOG_DEBUG, "Failed to load module %s: could not locate symbol 'exports'", modfile);
		dlclose(dl);
		return;
	}

	for(i = 0; harray[i] != NULL; i++) {
		count++;
	}

	handlers = realloc(handlers, (sizeof(struct handler *) * (handler_count + count + 1)));
	for(i = 0; harray[i] != NULL; i++) {
		h = malloc(sizeof(struct handler));
		h->name = strdup(harray[i]);
		h->func = dlsym(dl, harray[i]);
		handlers[handler_count + i] = h;
	}
	
	handler_count += count;
	handlers[handler_count] = NULL;

	dllist = realloc(dllist, sizeof(struct module *) * (call_count + 2));
	m = malloc(sizeof(struct module));
	m->name = strdup(modname);
	m->version = strdup(modver);
	m->destroyfunc = dlsym(dl, "destroy");
	m->dl = (void *) dl;
	dllist[call_count] = m;
	dllist[call_count + 1] = NULL;

	llog(LOG_DEBUG, "Calling module's init function.");
	(*initfunc)();

	call_count++;
}

void module_stats() {
	int i = 0;
	struct handler *h;

	llog(LOG_INFO, "---- I have %d loaded handlers ----", handler_count);
	for(i = 0; (h = handlers[i]) != NULL; i++) {
		llog(LOG_INFO, "%s", h->name);
	}
}

int has_handler(char *name) {
	int i, res;
	struct handler *h;

	if(name == NULL) {
		return 0;
	}

	if(handlers == NULL) {
		return 0;
	}

	for(i = 0; (h = handlers[i]) != NULL; i++) {
		res = strncmp(h->name, name, strlen(name));
		if(res == 0) {
			return 1;
		}
	}
	return 0;
}

void call_handler(char *name, xmlDocPtr doc, xmlNodePtr cur) {
	int i, res = 0;
	struct handler *h;

	for(i = 0; (h = handlers[i]) != NULL; i++) {
		res = strcmp(h->name, name);
		if(res == 0) {
			(*h->func)(doc, cur);
		}
	}
}

void unload_modules() {
	struct handler *h;
	int i, ret;
	struct module *m;
	void(*dfunc)();
	
	if(handlers != NULL) {
		for(i = 0; (h = handlers[i]) != NULL; i++) {
			free(h->name);
			free(h);
		}
		free(handlers);
	}

	if(dllist != NULL) {
		for(i = 0; (m = dllist[i]) != NULL; i++) {
			if(dfunc = m->destroyfunc) {
				llog(LOG_DEBUG, "Calling destroy function for module '%s'.", m->name);
				(*dfunc)();
			}
			llog(LOG_DEBUG, "Unloading module '%s', version %s.", m->name, m->version);
			free(m->name);
			free(m->version);
			ret = dlclose((void *) m->dl);
			if(ret != 0) {
				llog(LOG_ERR, "Error closing dl handle.");
			}
			free(m);
			llog(LOG_DEBUG, "Unload successfull.");
		}
		free(dllist);
	}
}
