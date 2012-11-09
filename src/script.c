#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "module.h"
#include "script.h"

void parse_commands(xmlDocPtr doc, xmlNodePtr cur) {
	int res;

	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if(strncmp(cur->name, "text", 4) && strncmp(cur->name, "comment", 7)) {
			res = has_handler((char *)cur->name);
			if(res == 1) {
				call_handler((char *)cur->name, doc, cur);
			} else {
				llog(LOG_ERR, "There is no handler for %s", (char *)cur->name);
			}	
		}
		cur = cur->next;
	}
	return;
}

int parse_doc(xmlDocPtr doc) {
	xmlNodePtr cur;

	cur = xmlDocGetRootElement(doc);
	if(cur == NULL) {
		llog(LOG_ERR, "Empty document");
		return 0;
	}

	if(xmlStrcmp(cur->name, (const xmlChar *) "script")) {
		llog(LOG_ERR, "Document of the wrong type");
		return 0;
	}

	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if((!xmlStrcmp(cur->name, (const xmlChar *)"commands"))) {
			parse_commands(doc, cur);
		}
		cur = cur->next;
	}
	return 1;
}
