#include <dirent.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>

void init(void);
static void get_packages(void);
void destroy(void);

char modname[] = "package operations";
char modver[] = "1.0.0";

char *exports[] = {
	"get_packages",
	NULL
};

void init(void) {
}

int get_packages() {
}

void destroy(void) {
}
