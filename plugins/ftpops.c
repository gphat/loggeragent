#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <curl/curl.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "log.h"
#include "util.h"

char modname[] = "ftp operations";
char modver[]  = "1.0.0";

char *exports[] = {
	"ftp_putfile",
	"ftp_getfile",
	NULL
};

void init() {
	/* We don't need to do anything here. */
}

void ftp_putfile(xmlDocPtr doc, xmlNodePtr cur) {
	xmlChar *remotehost, *filename;
	xmlChar *remotename, *remotedir;
	xmlChar *username, *password;
	CURL *c;
	CURLcode res;
	char *url, *userpwd, *needle, *sfilename, errmsg[CURL_ERROR_SIZE];
	FILE *filesrc;
	int hd;
	struct stat finfo;
	double speed, size;

	remotehost	= xmlGetProp(cur, (xmlChar *) "remotehost");
	if(remotehost == NULL) {
		llog(LOG_ERR, "ftp_putfile: Must specify a remotehost.");
		return;
	}

	filename	= xmlGetProp(cur, (xmlChar *) "filename");
	if(filename == NULL) {
		llog(LOG_ERR, "ftp_putfile: Must specify a filename.");
		return;
	}

	needle = strrchr(filename, '/');
	if(needle != NULL) {
		sfilename = needle;
		sfilename++;
		llog(LOG_DEBUG, "Name of file is '%s' minus directory.", sfilename);
	} else {
		sfilename = filename;
	}

	remotename	= xmlGetProp(cur, (xmlChar *) "remotename");
	remotedir	= xmlGetProp(cur, (xmlChar *) "remotedir");
	username	= xmlGetProp(cur, (xmlChar *) "username");
	if(username == NULL) {
		llog(LOG_ERR, "ftp_putfile: Must specify a username.");
		return;
	}

	password	= xmlGetProp(cur, (xmlChar *) "password");
	if(password == NULL) {
		llog(LOG_ERR, "ftp_putfile: Must specify a password.");
		return;
	}

	/* Create a user/password string with the user, then a colon, then the
	 * password.
	 */
	userpwd = lprintf("%s:%s", username, password);
	llog(LOG_DEBUG, "ftp_putfile: Username/Password string is %s", userpwd);

	/* If a remote file name was provided use it, otherwise use the name of
	 * the local file.
	 */
	if(remotedir != NULL) {
		if(remotename != NULL) {
			url = lprintf("ftp://%s/%s/%s", remotehost, remotedir, remotename);
		} else {
			url = lprintf("ftp://%s/%s/%s", remotehost, remotedir, sfilename);
		}
	} else {
		if(remotename != NULL) {
			url = lprintf("ftp://%s/%s", remotehost, remotename);
		} else {
			url = lprintf("ftp://%s/%s", remotehost, sfilename);
		}
	}
	
	llog(LOG_DEBUG, "ftp_putfile: URL is %s", url);

	hd = open(filename, O_RDONLY);
	fstat(hd, &finfo);

	filesrc = fdopen(hd, "r");
	if(filesrc == NULL) {
		llog(LOG_ERR, "ftp_putfile: Could not open local file '%s'.", filename);
		xmlFree(remotehost);
		xmlFree(filename);
		xmlFree(remotename);
		xmlFree(remotedir);
		xmlFree(username);
		xmlFree(password);
		free(userpwd);
		free(url);
		return;
	}

	c = curl_easy_init();
	if(c) {
		curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(c, CURLOPT_UPLOAD, 1);
		curl_easy_setopt(c, CURLOPT_URL, url);
		curl_easy_setopt(c, CURLOPT_READDATA, filesrc);
		curl_easy_setopt(c, CURLOPT_INFILESIZE, finfo.st_size);
		curl_easy_setopt(c, CURLOPT_ERRORBUFFER, &errmsg);
		res = curl_easy_perform(c);

		if(CURLE_OK != res) {
			curl_easy_getinfo(c, CURLINFO_SPEED_UPLOAD, &speed);
			curl_easy_getinfo(c, CURLINFO_SIZE_UPLOAD, &size);
			llog(LOG_DEBUG, "ftp_putfile: Speed was %.2lf bytes/sec, size was %.2lf bytes", speed, size);
		} else {
			llog(LOG_ERR, "putfile error: %s", errmsg);
		}

		curl_easy_cleanup(c);
	} else {
		llog(LOG_ERR, "ftp_putfile: Couldn't init curl for FTP connection.");
	}

	xmlFree(remotehost);
	xmlFree(filename);
	xmlFree(remotename);
	xmlFree(remotedir);
	xmlFree(username);
	xmlFree(password);

	free(userpwd);
	free(url);

	fclose(filesrc);
}

void ftp_getfile(xmlDocPtr doc, xmlNodePtr cur) {
	xmlChar *remotehost, *filename;
	xmlChar *localname, *remotedir;
	xmlChar *username, *password;
	xmlChar *localdir, *mode;
	CURL *c;
	CURLcode res;
	char *url, *userpwd, errmsg[CURL_ERROR_SIZE];
	FILE *filesrc;
	int hd;
	mode_t imode = 0644;
	struct stat finfo;
	double speed, size;

	remotehost = xmlGetProp(cur, (xmlChar *) "remotehost");
	if(remotehost == NULL) {
		llog(LOG_ERR, "ftp_getfile: Must specify a remotehost.");
		return;
	}

	filename	= xmlGetProp(cur, (xmlChar *) "filename");
	if(filename == NULL) {
		llog(LOG_ERR, "ftp_getfile: Must specify a filename.");
		return;
	}

	localname	= xmlGetProp(cur, (xmlChar *) "localname");
	remotedir	= xmlGetProp(cur, (xmlChar *) "remotedir");
	username	= xmlGetProp(cur, (xmlChar *) "username");
	if(username == NULL) {
		llog(LOG_ERR, "ftp_getfile: Must specify a username.");
		return;
	}

	password	= xmlGetProp(cur, (xmlChar *) "password");
	if(password == NULL) {
		llog(LOG_ERR, "ftp_getfile: Must specify a password.");
		return;
	}
	localdir	= xmlGetProp(cur, (xmlChar *) "localdir");
	mode		= xmlGetProp(cur, (xmlChar *) "mode");

	/* Create a user/password string with the user, then a colon, then the
	 * password.
	 */
	userpwd = lprintf("%s:%s", username, password);
	llog(LOG_DEBUG, "ftp_getfile: Username/Password string is %s", userpwd);

	/* Build the URL.
	 */
	if(remotedir != NULL) {
		url = lprintf("ftp://%s/%s/%s", remotehost, remotedir, filename);
	} else {
		url = lprintf("ftp://%s/%s", remotehost, filename);
	}
	llog(LOG_DEBUG, "ftp_getfile: URL is %s", url);

	if(mode != NULL) {
		imode = (mode_t) (strtol((const char *)mode, NULL, 8));
	}
	llog(LOG_DEBUG, "ftp_getfile: Mode is %o", imode);

	/* If a remote file name was provided use it, otherwise use the name of
	 * the local file.
	 */
	if(localname != NULL) {
		llog(LOG_DEBUG, "ftp_getfile: Different localname");
		hd = open(localname, O_CREAT | O_RDWR, imode);
	} else {
		hd = open(filename, O_CREAT | O_RDWR, imode);
	}
	fstat(hd, &finfo);

	filesrc = fdopen(hd, "w");
	if(filesrc == NULL) {
		llog(LOG_ERR, "ftp_getfile: Could not open local file '%s'.", filename);
		xmlFree(remotehost);
		xmlFree(filename);
		xmlFree(localname);
		xmlFree(remotedir);
		xmlFree(username);
		xmlFree(password);
		xmlFree(localdir);
		xmlFree(mode);

		free(userpwd);
		free(url);
		return;
	}

	c = curl_easy_init();
	if(c) {
		curl_easy_setopt(c, CURLOPT_USERPWD, userpwd);
		curl_easy_setopt(c, CURLOPT_URL, url);
		curl_easy_setopt(c, CURLOPT_WRITEDATA, filesrc);
		curl_easy_setopt(c, CURLOPT_ERRORBUFFER, &errmsg);
		res = curl_easy_perform(c);

		if(res == 0) {
			curl_easy_getinfo(c, CURLINFO_SPEED_DOWNLOAD, &speed);
			curl_easy_getinfo(c, CURLINFO_SIZE_DOWNLOAD, &size);
			llog(LOG_DEBUG, "ftp_getfile: Speed was %.2lf bytes/sec, size was %.2lf bytes", speed, size);
		} else {
			llog(LOG_ERR, errmsg);
		}

		curl_easy_cleanup(c);
	} else {
		llog(LOG_ERR, "ftp_getfile: Couldn't init curl");
	}

	fclose(filesrc);

	xmlFree(remotehost);
	xmlFree(filename);
	xmlFree(localname);
	xmlFree(remotedir);
	xmlFree(username);
	xmlFree(password);
	xmlFree(localdir);
	xmlFree(mode);

	free(userpwd);
	free(url);
}

void destroy() {
	/* Again, nothing. */
}
