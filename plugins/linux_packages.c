#include <fcntl.h>

#include <rpm/rpmlib.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

xmlDocPtr xml_get_package(void);

int main() {
	xml_get_package();
	exit(0);
}

xmlDocPtr xml_get_package(void) {
	Header h;
	rpmdbMatchIterator	mi;
	rpmdb db;
	const char *name, *version, *release;

	rpmReadConfigFiles(NULL, NULL);
	if(rpmdbOpen(NULL, &db, O_RDONLY, 0644) != 0) {
		printf("Cannot open database!");
		return NULL;
	}

	mi = rpmdbInitIterator(db, RPMDBI_PACKAGES, NULL, 0);	
	while((h= rpmdbNextIterator(mi)) != NULL) {
		rpmHeaderGetEntry(h, RPMTAG_NAME, NULL, (void **)&name, NULL);
		rpmHeaderGetEntry(h, RPMTAG_VERSION, NULL, (void **)&version, NULL);
		rpmHeaderGetEntry(h, RPMTAG_RELEASE, NULL, (void **)&release, NULL);
		printf("%s: %s-%s\n", name, version, release);
	}

	headerFree(h);
	rpmdbFreeIterator(mi);
	rpmdbClose(db);
}
