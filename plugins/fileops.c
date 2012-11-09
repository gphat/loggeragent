#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "log.h"
#include "util.h"

char modname[] = "file operations";
char modver[]  = "1.0.0";

char *exports[] = {
	"createfile",
	"removefile",
	"move",
	"removedir",
	"makedir",
	"mksymlink",
	"execute",
	"readfile",
	"writefile",
	"statfile",
	"list",
	NULL
};

void init() {
	/* Nothing */
}

void createfile(xmlDocPtr doc, xmlNodePtr cur) {
	int retval;
	xmlChar *filename;
	xmlChar *mode;
	mode_t imode = 0644;

	llog(LOG_DEBUG, "---- handler createfile ----");

	filename = xmlGetProp(cur, (xmlChar *) "filename");
	if(filename == NULL) {
		llog(LOG_ERR, "Must specify a filename.");
		return;
	}
	mode = xmlGetProp(cur, (xmlChar *) "mode");
	if(mode != NULL) {
		imode = (mode_t) (strtol((const char *)mode, NULL, 8));
	}
	llog(LOG_DEBUG, "Mode is %o", imode);

	retval = open((const char *)filename, O_CREAT | O_EXCL, imode);
	if(retval == -1) {
		llog(LOG_ERR, "open() failed: %s", strerror(errno));
	} else {
		llog(LOG_DEBUG, "open() succeeded");
	}

	xmlFree(filename);
	xmlFree(mode);
}

void removefile(xmlDocPtr doc, xmlNodePtr cur) {
	int retval;
	xmlChar *filename;

	filename = xmlGetProp(cur, (xmlChar *) "filename");
	if(filename == NULL) {
		llog(LOG_ERR, "removefile: Must specify a filename.");
		return;
	}
	llog(LOG_DEBUG, "I am removing '%s'", filename);
	retval = unlink((char *)filename);
	if(retval == -1) {
		llog(LOG_ERR, "removefile: unlink() failed: %s", strerror(errno));
	} else {
		llog(LOG_DEBUG, "removefile: unlink() succeeded");
	}
	xmlFree(filename);
}

void move(xmlDocPtr doc, xmlNodePtr cur) {
	int retval;
	xmlChar *fromname;
	xmlChar *toname;

	fromname = xmlGetProp(cur, (xmlChar *) "fromname");
	if(fromname == NULL) {
		llog(LOG_ERR, "move: Must specify a fromname.");
		return;
	}
	toname = xmlGetProp(cur, (xmlChar *) "toname");
	if(toname == NULL) {
		llog(LOG_ERR, "move: Must specify a toname.");
		return;
	}
	llog(LOG_DEBUG, "move: I am moving '%s' to '%s'", fromname, toname);
	retval = rename((char *)fromname, (char *)toname);
	if(retval == -1) {
		llog(LOG_ERR, "move: rename() failed: %s", strerror(errno));
	} else {
		llog(LOG_DEBUG, "move: rename() succeeded");
	}
	xmlFree(fromname);
	xmlFree(toname);
}

void removedir(xmlDocPtr doc, xmlNodePtr cur) {
	int retval;
	xmlChar *dirname;

	dirname = xmlGetProp(cur, (xmlChar *) "dirname");
	if(dirname == NULL) {
		llog(LOG_ERR, "removedir: Must specify a dirname.");
		return;
	}
	llog(LOG_DEBUG, "removedir: I am removing '%s'", dirname);
	retval = rmdir((char *)dirname);
	if(retval == -1) {
		llog(LOG_ERR, "removedir: rmdir() failed: %s", strerror(errno));
	} else {
		llog(LOG_DEBUG, "removedir: rmdir() succeeded");
	}
	xmlFree(dirname);
}

void makedir(xmlDocPtr doc, xmlNodePtr cur) {
	int retval;
	xmlChar *dirname;
	xmlChar *mode;
	mode_t imode = 0644;

	dirname = xmlGetProp(cur, (xmlChar *) "dirname");
	if(dirname == NULL) {
		llog(LOG_ERR, "makedir: Must specify a dirname.");
		return;
	}

	mode = xmlGetProp(cur, (xmlChar *) "mode");
	if(mode != NULL) {
		imode = (mode_t) (strtol((const char *)mode, NULL, 8));
	}

	llog(LOG_DEBUG, "makedir: I am creating '%s' with mode %o", dirname, imode);
	retval = mkdir((char *)dirname, imode);
	if(retval == -1) {
		llog(LOG_ERR, "makedir: mkdir() failed: %s", strerror(errno));
	} else {
		llog(LOG_DEBUG, "makedir: mkdir() succeeded");
	}
	xmlFree(dirname);
	xmlFree(mode);
}

void mksymlink(xmlDocPtr doc, xmlNodePtr cur) {
	int retval;
	xmlChar *oldpath;
	xmlChar *newpath;

	oldpath = xmlGetProp(cur, (xmlChar *) "oldpath");
	if(oldpath == NULL) {
		llog(LOG_ERR, "mksymlink: Must specify an oldpath.");
		return;
	}
	newpath = xmlGetProp(cur, (xmlChar *) "newpath");
	if(newpath == NULL) {
		llog(LOG_ERR, "mksymlink: Must specify an newpath.");
		return;
	}

	retval = symlink((char *)oldpath, (char *)newpath);
	if(retval == -1) {
		llog(LOG_ERR, "mksymlink: symlink() failed: %s", strerror(errno));
	} else {
		llog(LOG_ERR, "mksymlink: symlink() succeeded");
	}
	xmlFree(oldpath);
	xmlFree(newpath);
}

void execute(xmlDocPtr doc, xmlNodePtr cur) {
	xmlChar *directory;
	xmlChar *command;
	xmlChar *returnoutput;
	char cwd[2048];
	FILE *ptr;
	char buf[1024];
	int returnoutputflag = 0, status, retval = 0;

	/* Get the current directory, in case we chdir() */
	getcwd(cwd, sizeof(cwd));
	if(cwd == NULL) {
		llog(LOG_ERR, "execute: Could not get current directory.");
	}

	directory = xmlGetProp(cur, (xmlChar *) "directory");
	if(directory != NULL) {
		llog(LOG_DEBUG, "execute: I am changing directories");
		retval = chdir((char *)directory);
		if(retval < 0) {
			llog(LOG_ERR, "Could not chdir to '%s': %s", directory, strerror(errno));
		}
	}

	returnoutput = xmlGetProp(cur, (xmlChar *) "returnoutput");
	if(!(xmlStrcmp((xmlChar *) "yes", (xmlChar *) returnoutput))) {
		llog(LOG_DEBUG, "execute: I am returning output");
		returnoutputflag = 1;
	}

	command = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	llog(LOG_DEBUG, "execute: I am executing '%s'", command);
	if(returnoutputflag > 0) {
		ptr = popen((char *) command, "r");
		wait3(&status, 0, NULL);
		if(status != 0) {
			llog(LOG_ERR, "execute: popen() failed, bad exit status.");
		} else {
			if(ptr == NULL) {
				llog(LOG_ERR, "execute: popen() failed");
			} else {
				while(fgets(buf, 1024, ptr) != NULL) {
					add_output(buf);
				}
			}
		}
		pclose(ptr);
	} else {
		retval = system((char *) command);
		if(retval < 0) {
			llog(LOG_ERR, "Command failed to execute.");
		}
	}

	if(directory != NULL) {
		/* chdir() back to the original directory */
		if(cwd != NULL) {
			retval = chdir(cwd);
			if(retval < 0) {
				llog(LOG_ERR, "Could not chdir back to '%s': %s", directory, strerror(errno));
			}
		}
	}

	xmlFree(directory);
	xmlFree(command);
	xmlFree(returnoutput);
}

void writefile(xmlDocPtr doc, xmlNodePtr cur) {
	xmlChar *filename;
	xmlChar *contents;
	xmlChar *mode;
	FILE *ptr;
	int retval = 0;

	filename = xmlGetProp(cur, (xmlChar *) "filename");
	if(filename == NULL) {
		llog(LOG_ERR, "writefile: Must specify a filename.");
		return;
	}

	mode = xmlGetProp(cur, (xmlChar *) "mode");
	if(mode == NULL) {
		mode = malloc(sizeof(char) + 1);
		if(mode == NULL) {
			llog(LOG_ERR, "writefile: Could not allocate memory for mode.");
			xmlFree(filename);
			return;
		}
		mode = (xmlChar *) "a+";
	}

	contents = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
	if(contents == NULL) {
		llog(LOG_ERR, "writefile: Use createfile for empty files.");
		xmlFree(mode);
		xmlFree(filename);
		return;
	}

	ptr = fopen((const char *) filename, (char *) mode);

	if(ptr == NULL) {
		llog(LOG_ERR, "writefile: fopen() failed: %s", strerror(errno));
		xmlFree(mode);
		xmlFree(contents);
		xmlFree(filename);
		return;
	} else {
		retval = fputs((const char *) contents, ptr);
		if(retval < 0) {
			llog(LOG_ERR, "writefile: fputs() failed: %d", retval);
			xmlFree(mode);
			xmlFree(contents);
			xmlFree(filename);
			return;
		}
	}
	retval = fclose(ptr);
	if(retval != 0) {
		llog(LOG_ERR, "writefile: fclose() failed: %s", strerror(errno));
	}

	xmlFree(mode);
	xmlFree(contents);
	xmlFree(filename);
}

void statfile(xmlDocPtr doc, xmlNodePtr cur) {
	xmlChar *filename;
	struct stat buf;
	int retval;
	char *statstr;

	filename = xmlGetProp(cur, (xmlChar *) "filename");
	if(filename == NULL) {
		llog(LOG_ERR, "statfile: Must specify a filename.");
		return;
	}

	retval = stat((const char *) filename, &buf);

	if(retval == -1) {
		llog(LOG_ERR, "statfile: stat() failed: %s", strerror(errno));
		xmlFree(filename);
		return;
	} else {
		statstr = lprintf("UID:%d,GID:%d,Size:%d,Mode:%o\n", buf.st_uid, buf.st_gid, buf.st_size, buf.st_mode);
		add_output(statstr);
		free(statstr);
	}

	xmlFree(filename);
}

void list(xmlDocPtr doc, xmlNodePtr cur) {
	xmlChar *directory;
	struct dirent *ent;
	DIR *dir;
	char *str;
	int retval = 0;

	directory = xmlGetProp(cur, (xmlChar *) "directory");
	if(directory == NULL) {
		llog(LOG_ERR, "list: Must specify a directory.");
		return;
	}

	dir = opendir((const char *) directory);

	if(dir == NULL) {
		llog(LOG_ERR, "list: opendir() failed: %s", strerror(errno));
		xmlFree(directory);
		return;
	} else {
		while((ent = readdir(dir)) != NULL) {
			str = lprintf("%s\n", ent->d_name);
			add_output(str);
			free(str);
			ent = readdir(dir);
		}
	}
	retval = closedir(dir);
	if(retval < 0) {
		llog(LOG_ERR, "list: closedir() failed: %s", retval);
	}

	xmlFree(directory);
}

void destroy() {
}
