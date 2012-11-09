#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include <kstat.h>

#include "log.h"
#include "util.h"

#define DUMBBUFF 100

void init(void);
static void add_disk_info(xmlNodePtr, kstat_t *);
void destroy(void);

char modname[] = "solaris devices";
char modver[] = "1.0.0";

char *exports[] = {
	"cpuinfo",
	"diskinfo",
	NULL
};

static ulong_t *get_named_member(kstat_t *, char *);

void init(void) {
}

int cpuinfo(void) {
	int 		size;
	kstat_ctl_t	*kc;
	kstat_t		*kp;
	kstat_named_t	*kn;
	xmlDocPtr	doc;
	xmlNodePtr	root, cur;
	xmlChar 	*cpuinfo;
	char		buf[DUMBBUFF];

	memset(buf, 0, DUMBBUFF);

	if(!(kc = kstat_open())) {
		printf("Failed to open kstat.\n");
		return -1;
	}

	if(!(kp = kstat_lookup(kc, "cpu_info", -1, NULL))) {
		printf("Failed to lookup cpu_info");
		return -1;
	}

	doc = xmlNewDoc((const xmlChar *) "1.0");
	root = xmlNewDocNode(doc, NULL, (const xmlChar *) "cpuinfo", NULL);

	xmlDocSetRootElement(doc, root);

	for(kp; kp; kp = kp->ks_next) {
		if(strcmp(kp->ks_module, "cpu_info") != 0) {
			continue;
		}
		
		kstat_read(kc, kp, 0);

		cur = xmlNewChild(root, NULL, (const xmlChar *) "cpu", NULL);

		xmlNewChild(cur, NULL, (const xmlChar *) "instance", kp->ks_name);
		kn = (kstat_named_t *)kstat_data_lookup(kp, "clock_MHz");
		snprintf(buf, DUMBBUFF, "%lu", kn->value.ul);
		xmlNewChild(cur, NULL, (const xmlChar *) "mhz", buf);

		kn = (kstat_named_t *)kstat_data_lookup(kp, "cpu_type");
		xmlNewChild(cur, NULL, (const xmlChar *) "type", kn->value.c);

		kn = (kstat_named_t *)kstat_data_lookup(kp, "state");
		xmlNewChild(cur, NULL, (const xmlChar *) "state", kn->value.c);
	}

	xmlDocDumpMemory(doc, &cpuinfo, &size);
	xmlFreeDoc(doc);
	kstat_close(kc);

	printf("%s\n", cpuinfo);

	return 0;
}

int main(void) {
	int 		size;
	kstat_ctl_t	*kc;
	kstat_t		*kp;
	kstat_named_t	*kn;
	xmlDocPtr	doc;
	xmlNodePtr	root, cur;
	xmlChar 	*diskinfo;

	if(!(kc = kstat_open())) {
		printf("Failed to open kstat.\n");
		return -1;
	}

	doc = xmlNewDoc((const xmlChar *) "1.0");
	root = xmlNewDocNode(doc, NULL, (const xmlChar *) "diskinfo", NULL);

	xmlDocSetRootElement(doc, root);

	if(kp = kstat_lookup(kc, "daderror", -1, NULL)) {
		for(kp; kp; kp = kp->ks_next) {
			kstat_read(kc, kp, 0);
			if(strcmp(kp->ks_module, "daderror") != 0) {
				continue;
			}
			add_disk_info(root, kp);
		}
	}

	if(kp = kstat_lookup(kc, "ssderr", -1, NULL)) {
		for(kp; kp; kp = kp->ks_next) {
			kstat_read(kc, kp, 0);
			if(strcmp(kp->ks_module, "ssderr") != 0) {
				continue;
			}
			add_disk_info(root, kp);
		}
	}

	if(kp = kstat_lookup(kc, "sderr", -1, NULL)) {
		for(kp; kp; kp = kp->ks_next) {
			kstat_read(kc, kp, 0);
			if(strcmp(kp->ks_module, "sderr") != 0) {
				continue;
			}
			add_disk_info(root, kp);
		}
	}

	kstat_close(kc);
	xmlDocDumpMemory(doc, &diskinfo, &size);
	xmlFreeDoc(doc);

	printf("%s\n", diskinfo);
	return 0;
}

static void add_disk_info(xmlNodePtr root, kstat_t *kp) {
	kstat_named_t	*kn;
	xmlNodePtr	cur;
	char		buf[DUMBBUFF];

	cur = xmlNewChild(root, NULL, (const xmlChar *) "disk", NULL);

	xmlNewChild(cur, NULL, (const xmlChar *) "Name", kp->ks_name);

	kn = (kstat_named_t *)kstat_data_lookup(kp, "Device Not Ready");
	if(kn != NULL) {
		snprintf(buf, DUMBBUFF, "%lu", kn->value.ul);
		xmlNewChild(cur, NULL, (const xmlChar *) "NotReady", buf);
	}

	kn = (kstat_named_t *)kstat_data_lookup(kp, "Hard Errors");
	if(kn != NULL) {
		snprintf(buf, DUMBBUFF, "%lu", kn->value.ul);
		xmlNewChild(cur, NULL, (const xmlChar *) "HardErrors", buf);
	}

	kn = (kstat_named_t *)kstat_data_lookup(kp, "Illegal Request");
	if(kn != NULL) {
		snprintf(buf, DUMBBUFF, "%lu", kn->value.ul);
		xmlNewChild(cur, NULL, (const xmlChar *) "IllegalRequests", buf);
	}

	kn = (kstat_named_t *)kstat_data_lookup(kp, "Media Error");
	if(kn != NULL) {
		snprintf(buf, DUMBBUFF, "%lu", kn->value.ul);
		xmlNewChild(cur, NULL, (const xmlChar *) "MediaErrors", buf);
	}

	kn = (kstat_named_t *)kstat_data_lookup(kp, "Predictive Failure Analysis");
	if(kn != NULL) {
		snprintf(buf, DUMBBUFF, "%lu", kn->value.ul);
		xmlNewChild(cur, NULL, (const xmlChar *) "PredictiveFailureAnalysis", buf);
	}

	kn = (kstat_named_t *)kstat_data_lookup(kp, "Recoverable");
	if(kn != NULL) {
		snprintf(buf, DUMBBUFF, "%lu", kn->value.ul);
		xmlNewChild(cur, NULL, (const xmlChar *) "Recoverable", buf);
	}

	kn = (kstat_named_t *)kstat_data_lookup(kp, "Product");
	if(kn != NULL) {
		memcpy(buf, kn->value.c, 16);
		//buf[16] = '\0';
		trim(buf);
		xmlNewChild(cur, NULL, (const xmlChar *) "Product", buf);
	}

	memset(buf, 0, 16);
	kn = (kstat_named_t *)kstat_data_lookup(kp, "Vendor");
	if(kn != NULL) {
		memcpy(buf, kn->value.c, 16);
		//buf[16] = '\0';
		trim(buf);
		xmlNewChild(cur, NULL, (const xmlChar *) "Vendor", buf);
	}

	memset(buf, 0, 16);
	kn = (kstat_named_t *)kstat_data_lookup(kp, "Serial No");
	if(kn != NULL) {
		memcpy(buf, kn->value.c, 16);
		//buf[16] = '\0';
		trim(buf);
		xmlNewChild(cur, NULL, (const xmlChar *) "SerialNo", buf);
	}
}

static ulong_t *get_named_member(kstat_t *kp, char *name) {
	kstat_named_t *kn;

	kn = (kstat_named_t *) kstat_data_lookup(kp, name);
	if(kn == 0) {
		printf("Can not find member: %s\n", name);
		return (ulong_t *) -1;
	}

	return &kn->value.ul;
}

void destroy() {
}
