#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "configfile.h"
#include "log.h"

static xmlDocPtr doc;
static xmlXPathContextPtr xpathCtx;

int read_config(void) {
	struct stat stats;
	int ret;

	llog(LOG_DEBUG, "Reading configuration file.");
	
	ret = stat("/etc/loggeragent.xml", &stats);
	if(ret == -1) {
		llog(LOG_ERR, "Could not open config file: %s", strerror(errno));
		return -1;
	}

	xmlInitParser();
	xmlXPathInit();

	doc = xmlParseFile("/etc/loggeragent.xml");
	if(doc == NULL) {
		llog(LOG_ERR, "Error parsing configuration file.");
		return(-1);
	}

	xpathCtx = xmlXPathNewContext(doc);
	if(xpathCtx == NULL) {
		llog(LOG_ERR, "Error creating new XPath context.");
		xmlFreeDoc(doc);
		return(-1);
	}

	return 0;
}

char *get_config_value(char *name) {
	xmlXPathObjectPtr result;
	char *ret;
	char *temp;

	if(name == NULL) {
		return NULL;
	}
	
	result = xmlXPathEval(name, xpathCtx);
	ret = (char *) xmlXPathCastToString(result);
	xmlXPathFreeObject(result);
	if(!strcmp(ret, "")) {
		 xmlFree(ret);
		return NULL;
	} else {
		 temp = strdup((char *) ret);
		 xmlFree(ret);
		 return temp;
	}
}

void free_config(void) {

	xmlXPathFreeContext(xpathCtx);
	xmlFreeDoc(doc);
	xmlCleanupParser();
}
