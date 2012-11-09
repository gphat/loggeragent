#include <dirent.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <netinet/in.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "config.h"
#include "configfile.h"
#include "job.h"
#include "log.h"
#include "network.h"
#include "metricops.h"
#include "util.h"

#define DUMBBUFF 100

void init(void);
static void send_saved_metrics(void);
void save_metric(char *);
void metric_sender(void);
static int send_metric(char *);
static char *lstat_to_string(struct s_stats *);
static void lstat_free(struct s_stats);
void destroy(void);

char modname[] = "metric operations";
char modver[] = "1.0.0";

char *exports[] = {
	"getmetrics",
	NULL
};

void init(void) {
	int interval = 0;
	char *mint = NULL;

	mint = get_config_value("/configuration/metricinterval");
	if(mint == NULL) {
		llog(LOG_ERR, "No metric interval, not adding a job.");
		return;
	}
	interval = atoi(mint);
	free(mint);
	
	if(!(interval > 0)) {
		llog(LOG_ERR, "Metric interval is invalid.");
	}
	llog(LOG_DEBUG, "Metric interval is %d.", interval);

	if(add_job("Metric Sender", &metric_sender, interval) != 1) {
		llog(LOG_ERR, "Job addition failed.");
	}
}

static void send_saved_metrics(void) {
	DIR *saved = NULL;
	FILE *savedfile = NULL;
	int ret = 0, count = 0;
	size_t pathlen = 0;
	struct dirent *ent;
	char *needle = NULL, *metfile = NULL, *savedir = NULL;
	char *xmlbuf = NULL;
	char xmltemp[16384];

	savedir = get_config_value("/configuration/metricsavepath");
	if(savedir == NULL) {
		llog(LOG_DEBUG, "Not checking for saved metrics, metricsavepath is NULL.");
		return;
	}

	saved = opendir(savedir);
	if(saved == NULL) {
		llog(LOG_ERR, "opendir(%s) failed: %s", savedir, strerror(errno));
		free(savedir);
		return;
	}

	llog(LOG_DEBUG, "Reading save directory, %s.", savedir);
	while((ent = readdir(saved)) != NULL) {
		needle = strstr(ent->d_name, ".xml");
		if((needle != NULL) && (strlen(needle) == 4)) {
			llog(LOG_DEBUG, "Found file '%s'.", ent->d_name);
			if(ent->d_name == NULL) {
				continue;
			}
			pathlen = strlen(savedir) + strlen(ent->d_name) + 2;
			metfile = malloc(sizeof(char) * pathlen);
			if(metfile == NULL) {
				llog(LOG_ERR, "Could not allocate memory for metfile.");
				free(savedir);
				closedir(saved);
				return;
			}
			snprintf(metfile, pathlen, "%s/%s", savedir, ent->d_name);
			llog(LOG_DEBUG, "Attempting to open and send %s", metfile);
			savedfile = fopen(metfile, "r");
			if(savedfile == NULL) {
				llog(LOG_DEBUG, "Failed to open %s.", metfile);
				closedir(saved);
				free(metfile);
				free(savedir);
				return;
			}
			while((fgets(xmltemp, 16383, savedfile)) != NULL) {
				if(xmlbuf == NULL) {
					xmlbuf = strdup(xmltemp);
				} else {
					if(xmltemp == NULL) {
						continue;
					}
					xmlbuf = realloc(xmlbuf, (sizeof(char *) * ((strlen(xmlbuf) + strlen(xmltemp) + 1))));
					if(xmlbuf == NULL) {
						llog(LOG_ERR, "Could not realloc() xmlbuffer.");
						closedir(saved);
						free(metfile);
						free(savedir);
						return;
					}
					xmlbuf = strncat(xmlbuf, xmltemp, strlen(xmltemp));
				}
			}

			ret = fclose(savedfile);
			if(ret == EOF) {
				llog(LOG_ERR, "Error closing '%s'.", metfile);
				closedir(saved);
				free(metfile);
				free(savedir);
				return;
			}

			if(xmlbuf != NULL) {
				sleep(1);
				ret = send_metric(xmlbuf);
			}

			free(xmlbuf);
			xmlbuf = NULL;
			if(ret == -1) {
				llog(LOG_DEBUG, "Resend of metric failed.");
			} else {
				llog(LOG_INFO, "%s resent successfully, removing.", metfile);
				ret = unlink(metfile);
				if(ret == -1) {
					llog(LOG_ERR, "Error unlinking '%s': %s", metfile, strerror(errno));
					closedir(saved);
					free(metfile);
					free(savedir);
					return;
				}
			}
			free(metfile);
			memset(xmltemp, 0, 16384);
			count++;
		}
	}
	ret = closedir(saved);
	free(savedir);
	if(ret == -1) {
		llog(LOG_ERR, "Error closing save directory.");
		return;
	}
	llog(LOG_DEBUG, "Sent %d saved metrics.", count);
}

void save_metric(char *metric) {
	int ret, metfd, savelimit = 5, savedcount = 0;
	struct tm *curtime;
	time_t now;
	char filename[24];
	char *savepath, *savedir, *needle, *limit;
	DIR *saved = NULL;
	struct dirent *ent;

	if(metric == NULL) {
		return;
	}

	llog(LOG_DEBUG, "Attempting to save metric.");

	savedir = get_config_value("/configuration/metricsavepath");
	saved = opendir(savedir);
	if(saved == NULL) {
		llog(LOG_ERR, "opendir(%s) failed: %s", savedir, strerror(errno));
		free(savedir);
		return;
	}

	llog(LOG_DEBUG, "Reading save directory, %s.", savedir);
	while((ent = readdir(saved)) != NULL) {
		needle = strstr(ent->d_name, ".xml");
		if((needle != NULL) && (strlen(needle) == 4)) {
			savedcount++;
		}
	}

	ret = closedir(saved);
	if(ret == -1) {
		free(savedir);
		llog(LOG_ERR, "Error closing save directory.");
		free(savedir);
		return;
	}

	limit = get_config_value("/configuration/metricsavelimit");
	if(limit != NULL) {
		savelimit = atoi(limit);
		free(limit);
	}

	if(savedcount >= savelimit) {
		llog(LOG_ERR, "METRIC LOSS: Too many existing saves (%d), skipping.", savedcount);
		free(savedir);
		return;
	}

	now = time(NULL);
	curtime = localtime(&now);
	snprintf(filename, 24, "%d-%02d-%02d_%02d:%02d:%02d.xml", (curtime->tm_year + 1900), curtime->tm_mon, curtime->tm_mday, curtime->tm_hour, curtime->tm_min, curtime->tm_sec);
	if(filename == NULL) {
		free(savedir);
		llog(LOG_ERR, "Could not build filename.");
		return;
	}
	savepath = malloc(sizeof(char) * ((strlen(savedir) + strlen(filename) + 2)));
	if(savepath == NULL) {
		free(savedir);
		llog(LOG_ERR, "Could not allocate memory for savepath.");
		return;
	}
	snprintf(savepath, (strlen(savedir) + 2 + strlen(filename)), "%s/%s", savedir, filename);
	llog(LOG_DEBUG, "Saving metric as %s", savepath);

	metfd = open(savepath, O_RDWR | O_CREAT | O_EXCL, S_IRWXU);
	if(metfd == -1) {
		llog(LOG_ERR, "Failed to open file for metric saving!");
		free(savedir);
		free(savepath);
		return;
	}
	ret = write(metfd, metric, strlen(metric));
	if(ret == -1) {
		llog(LOG_ERR, "Could not write metric to file:");
		free(savepath);
		free(savedir);
		return;
	} else {
		llog(LOG_INFO, "Wrote %d bytes to saved metric %s.", ret, filename);
	}
	close(metfd);

	free(savepath);
	free(savedir);
	return;
}

void metric_sender(void) {
	int 					ret;
	struct s_stats		s_lstats;
	char 					*met;

	ret = gather(&s_lstats);
	if(ret < 0) {
		llog(LOG_ERR, "Metric gathering returned with errors.");
		lstat_free(s_lstats);
		return;
	}
	met = lstat_to_string(&s_lstats);
	if(met != NULL) {
		ret = send_metric(met);
		if(ret < 0) {
			llog(LOG_ERR, "The metric did not send correctly.");
			save_metric(met);
		} else {
			llog(LOG_INFO, "Metric sent successfully.");
			send_saved_metrics();
		}
		free(met);
	}
	lstat_free(s_lstats);
}

void getmetrics(xmlDocPtr doc, xmlNodePtr cur) {
	char *metric = NULL;
	struct s_stats s_lstats;
	int ret = 0;

	ret = gather(&s_lstats);
	if(ret < 0) {
		llog(LOG_ERR, "Error gathering metrics.");
		lstat_free(s_lstats);
		return;
	}

	metric = lstat_to_string(&s_lstats);

	add_output(metric);

	free(metric);

	lstat_free(s_lstats);
}

static int send_metric(char *metric) {
	SSL_CTX		*ctx;
	SSL			*ssl;
	int			client, retval, portnum;
	char		*hostname, *portstring;

	if(metric == NULL) {
		return -1;
	}

	hostname = get_config_value("/configuration/metrichost");
	portstring = get_config_value("/configuration/metricport");

	if((hostname == NULL) || (portstring == NULL)) {
		llog(LOG_ERR, "metrichost and/or metricport are NULL.");
		return -1;
	}
	portnum = atoi(portstring);
	free(portstring);

	llog(LOG_DEBUG, "Attempting to send metric to %s:%d.", hostname, portnum);
	
	ctx = init_context();
	if(retval == -1) {
		llog(LOG_ERR, "Error retrieving context.");
		free(hostname);
		return -1;
	}
	client = open_connection(hostname, portnum);
	if(client == -1) {
		llog(LOG_ERR, "Error in open_connection().");
		free(hostname);
		return -1;
	}
	ssl = SSL_new(ctx);
	retval = SSL_set_fd(ssl, client);
	if(retval == 0) {
		llog(LOG_ERR, "Error setting fd for SSL: %s", ERR_error_string(ERR_get_error(), NULL));
		close(client);
		free(hostname);
		SSL_free(ssl);
		return -1;
	}
	if(SSL_connect(ssl) == -1) {
		llog(LOG_ERR, "Error connecting: %s", ERR_error_string(ERR_get_error(), NULL));
		close(client);
		free(hostname);
		SSL_free(ssl);
		return -1;
	} else {
		llog(LOG_DEBUG, "Connected to host '%s' for metric sending.", hostname);
		SSL_write(ssl, metric, (int) strlen(metric));
		SSL_write(ssl, "\r\n", 2);
		llog(LOG_DEBUG, metric);
		llog(LOG_DEBUG, "metric -->");
		SSL_write(ssl, "+DONE+\r\n", 8);
		llog(LOG_DEBUG, "+DONE+ -->");
	}
	free(hostname);
	SSL_free(ssl);
	retval = close(client);
	if(retval == -1) {
		llog(LOG_ERR, "Error closing socket.");
		return -1;
	}

	return 1;
}

static char *lstat_to_string(struct s_stats *s_lstats) {
	int					i, size;
	struct s_interface	*s_if;
	struct s_fs			*s_fs;
	xmlDocPtr			doc;
	xmlNodePtr			root, cur, cursub, cursubsub;
	char				buf[DUMBBUFF];
	xmlChar				*metric;
	/*
	 * The buf[DUMBBUFF] is a limitation of this function.  I didn't want to
	 * allocate all types of memory for each number that has to be converted
	 * to an xmlChar * for the xmlNewChild function, so I made a fixed
	 * buffer.
	 */

	doc = xmlNewDoc((const xmlChar *) "1.0");
	root = xmlNewDocNode(doc, NULL, (const xmlChar *) "metric", NULL);
	
	xmlDocSetRootElement(doc, root);

	cur = xmlNewChild(root, NULL, (const xmlChar *) "host", (xmlChar *) s_lstats->hostname);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "agent", (xmlChar *) PACKAGE_VERSION);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "system", (xmlChar *) s_lstats->sysname);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "release", (xmlChar *) s_lstats->release);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "version", (xmlChar *) s_lstats->version);

	snprintf(buf, DUMBBUFF, "%lu", s_lstats->time);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "timestamp", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->uptime);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "uptime", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%llu", s_lstats->availrealmem);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "avail_real", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%llu", s_lstats->totalrealmem);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "total_real", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->sharedmem);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "shared_mem", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->buffermem);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "buffer_mem", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->cachedmem);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "cached_mem", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%llu", s_lstats->availswap);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "avail_swap", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%llu", s_lstats->totalswap);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "total_swap", (xmlChar *) buf);

	/* Load Averages */
	cur = xmlNewChild(root, NULL, (const xmlChar *) "load", NULL);
	snprintf(buf, DUMBBUFF, "%.2f", s_lstats->load1);
	cursub = xmlNewChild(cur, NULL, (const xmlChar *) "one", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%.2f", s_lstats->load5);
	cursub = xmlNewChild(cur, NULL, (const xmlChar *) "five", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%.2f", s_lstats->load15);
	cursub = xmlNewChild(cur, NULL, (const xmlChar *) "fifteen", (xmlChar *) buf);

	snprintf(buf, DUMBBUFF, "%lu", s_lstats->processes);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "processes", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%d", s_lstats->numcpus);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "numcpus", (xmlChar *) buf);

	/* CPU Totals */
	cur = xmlNewChild(root, NULL, (const xmlChar *) "cpu_totals", NULL);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->cpusys);
	cursub = xmlNewChild(cur, NULL, (const xmlChar *) "sys", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu" , s_lstats->cpunice);
	cursub = xmlNewChild(cur, NULL, (const xmlChar *) "nice", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->cpuuser);
	cursub = xmlNewChild(cur, NULL, (const xmlChar *) "user", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->cpuidle);
	cursub = xmlNewChild(cur, NULL, (const xmlChar *) "idle", (xmlChar *) buf);

	snprintf(buf, DUMBBUFF, "%lu", s_lstats->pagesin);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "pages_in", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->pagesout);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "pages_out", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->swapsin);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "swap_in", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->swapsout);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "swap_out", (xmlChar *) buf);
	snprintf(buf, DUMBBUFF, "%lu", s_lstats->contexts);
	cur = xmlNewChild(root, NULL, (const xmlChar *) "contexts", (xmlChar *) buf);

	cur = xmlNewChild(root, NULL, (const xmlChar *) "interfaces", NULL);

	for(i = 0; (s_if = s_lstats->interfaces[i]) != NULL; i++) {
		cursub = xmlNewChild(cur, NULL, (const xmlChar *) "interface", NULL);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "device", (const xmlChar *) s_if->device);
		//cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "address", (const xmlChar *) s_if->address);
		snprintf(buf, DUMBBUFF, "%lu", s_if->rxbytes);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "rxbytes", (xmlChar *) buf);
		snprintf(buf, DUMBBUFF, "%lu", s_if->rxpackets);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "rxpackets", (xmlChar *) buf);
		snprintf(buf, DUMBBUFF, "%lu", s_if->txbytes);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "txbytes", (xmlChar *) buf);
		snprintf(buf, DUMBBUFF, "%lu", s_if->txpackets);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "txpackets", (xmlChar *) buf);
	}

	cur = xmlNewChild(root, NULL, (const xmlChar *) "filesystems", NULL);
	for(i = 0; (s_fs = s_lstats->filesystems[i]) != NULL; i++) {
		cursub = xmlNewChild(cur, NULL, (const xmlChar *) "filesystem", NULL);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "mountpoint", (xmlChar *) s_fs->mountpoint);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "device", (xmlChar *) s_fs->device);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "type", (xmlChar *) s_fs->fs);
		snprintf(buf, DUMBBUFF, "%lu", s_fs->blocks);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "blocks", (xmlChar *) buf);
		snprintf(buf, DUMBBUFF, "%lu", s_fs->blocksfree);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "blocks_free", (xmlChar *) buf);
		snprintf(buf, DUMBBUFF, "%lu", s_fs->blocksize);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "block_size", (xmlChar *) buf);
		snprintf(buf, DUMBBUFF, "%lu", s_fs->inodes);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "inodes", (xmlChar *) buf);
		snprintf(buf, DUMBBUFF, "%lu", s_fs->inodesfree);
		cursubsub = xmlNewChild(cursub, NULL, (const xmlChar *) "inodes_free", (xmlChar *) buf);
	}

	xmlDocDumpMemory(doc, &metric, &size);
	xmlFreeDoc(doc);
	return (char *) metric;
}

/*
 * Free the memory from an s_stats
 */
static void lstat_free(struct s_stats s_lstats) {
	struct s_interface	*s_if;
	struct s_fs			*s_fs;
	int					i;

	free(s_lstats.sysname);
	free(s_lstats.hostname);
	free(s_lstats.release);
	free(s_lstats.version);

	for(i = 0; (s_if = s_lstats.interfaces[i]) != NULL; i++) {
		free(s_if->device);
		if(s_if->address != NULL) {
			free(s_if->address);
		}
		free(s_if);
	}
	for(i = 0; (s_fs = s_lstats.filesystems[i]) != NULL; i++) {
		free(s_fs->fs);
		free(s_fs->device);
		free(s_fs->mountpoint);
		free(s_fs);
	}
	free(s_lstats.interfaces);
	free(s_lstats.filesystems);
}

void destroy() {
}
