#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

xmlDocPtr get_packages() {
	DIR *pkgdir;
	FILE *pkginfo;
	struct dirent *ent;
	char *pkgpath;
	int pkgpathlen = 0;
	const char *varsadm = "/var/sadm/pkg";
	char linetemp[16384];
	char *temp;

	pkgdir = opendir(varsadm);
	while((ent = readdir(pkgdir)) != NULL) {
		if(strncmp(ent->d_name, ".", 1) == 0) {
			continue;
		}
		/* Path + "/" + Filename + "/" + "pkginfo" + NULL */
		pkgpathlen = strlen(varsadm) + strlen(ent->d_name) + 10;
		pkgpath = (char *) malloc(sizeof(char) * pkgpathlen);
		snprintf(pkgpath, pkgpathlen, "%s/%s/pkginfo", varsadm, ent->d_name);
		pkginfo = fopen(pkgpath, "r");
		if(pkginfo == NULL) {
			printf("Failed to open %s.\n", pkgpath);
			free(pkgpath);
			exit(-1);
		}		
		while((fgets(linetemp, 16383, pkginfo)) != NULL) {
			if(!strncmp(linetemp, "NAME=", 5)) {
				temp = strchr(linetemp, '=');
				temp++;
				printf("%s: %s", ent->d_name, temp);
			}
			if(!strncmp(linetemp, "VERSION=", 8)) {
				temp = strchr(linetemp, '=');
				temp++;
				printf("\tVersion: %s", temp);
			}
			if(!strncmp(linetemp, "DESC=", 5)) {
				temp = strchr(linetemp, '=');
				temp++;
				printf("\tDescription: %s", temp);
			}
			if(!strncmp(linetemp, "INSTDATE=", 9)) {
				temp = strchr(linetemp, '=');
				temp++;
				printf("\tInstall Date: %s", temp);
			}
		}
		fclose(pkginfo);
		free(pkgpath);
	}
}
