#include <errno.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "config.h"
#include "configfile.h"
#include "job.h"
#include "log.h"
#include "module.h"
#include "network.h"

#define PIDFILE "/var/run/loggeragent.pid"

static SSL_CTX *ctx;
static int server;

static int logval = 0;
static int nodaemon = 0;

static char *progname = NULL;
static char **progargs = NULL;

void harakiri(int) __attribute__ ((__noreturn__));
void daemonize(void);
void show_usage(void);

int main(int argc, char **argv) {
	int ret, c, portnum;
	char *portstring;
	char buf[1024];
	char *job_interval;
	int jinterval = 60;

	if(argc > 0) {
		progname = argv[0];
		progargs = argv;
	}

	while((c = getopt(argc, argv, "+dhnv")) != -1) {
		switch(c) {
			case 'd':
				logval = logval | TO_DEBUG;
				break;
			case 'h':
				show_usage();
				break;
			case 'n':
				nodaemon = 1;
				logval = logval | TO_STDERR;
				break;
			case 'v':
				printf("%s\n", PACKAGE_STRING);
				exit(0);
				break;
		}
	}

	init_log(logval);

	ret = read_config();
	if(ret == -1) {
		llog(LOG_ERR, "Error reading configuration file.");
		exit(-1);
	}

	llog(LOG_INFO, "Starting %s", PACKAGE_STRING);

	load_modules();

	if(!nodaemon) {
		daemonize();
	}

	signal(SIGINT, harakiri);
	signal(SIGTERM, harakiri);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGHUP, harakiri);
	portstring = get_config_value("/configuration/listenport");
	if(portstring == NULL) {
		llog(LOG_ERR, "NULL listenport value, not listening.");
		if(get_job_count() == 0) {
			llog(LOG_ERR, "Exiting, since I'm not listening and have no jobs.");
			harakiri(0);
		}
	} else {
		portnum = atoi(portstring);
		free(portstring);
	}

	job_interval = get_config_value("/configuration/jobinterval");
	if(job_interval != NULL) { 
		llog(LOG_DEBUG, "Changed job interval to %s.", job_interval);
		jinterval = atoi(job_interval);
		free(job_interval);
	}

	ctx = init_context();
	if(ctx == NULL) {
		llog(LOG_ERR, "Could not create SSLv3 context.");
		harakiri(0);
	}

	server = open_listener(portnum);
	if(server == -1) {
		llog(LOG_ERR, "FATAL: Could not bind to port '%s'.", portnum);
		harakiri(0);
	}
	llog(LOG_INFO, "Listening on port %d.", portnum);

	llog(LOG_DEBUG, "---- main loop ----");
	while(1) {
		struct sockaddr_in addr;
		int len = sizeof(addr);
		fd_set rfds;
		int retval;
		struct timeval tv;
		SSL *ssl;

		FD_ZERO(&rfds);

		FD_SET(server, &rfds);
		tv.tv_sec = jinterval;
		tv.tv_usec = 0;

		/* Thanks to timball for the idea to use select() here, which led
		 * to the job ideas.
		 */
		retval = select((1 + (int) server), &rfds, NULL, NULL, &tv);

		if(retval) {
			int client = accept(server, (struct sockaddr *)&addr, &len);
			if(client == -1) {
				llog(LOG_ERR, "Error in accept()");
				harakiri(0);
			}
			llog(LOG_INFO, "Connection from %s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
			ssl = SSL_new(ctx);
			if(ssl == NULL) {
				ERR_error_string(ERR_get_error(), buf);
				llog(LOG_ERR, "SSL_new() failed: %s", buf);
				harakiri(0);
			}
			ret = SSL_set_fd(ssl, client);
			if(ret == 0) {
				ERR_error_string(ERR_get_error(), buf);
				llog(LOG_ERR, "SSL_set_fd() failed: %s", buf);
				harakiri(0);
			}
			servlet(ssl);
			close(client);
			SSL_shutdown(ssl);
			SSL_free(ssl);
		} else {
			llog(LOG_DEBUG, "Timer expired, checking jobs.");
			run_jobs();
		}
	}

	harakiri(0);
	return(1);
}

void daemonize() {
	FILE *stream;
	struct stat *buf;
	int ret;

	switch(fork()) {
		case -1:
			llog(LOG_ERR, "Could not fork for daemonization.");
			exit(-1);
		case 0:
			fclose(stdin);
			fclose(stdout);
			fclose(stderr);
			if(setsid() == -1) {
				llog(LOG_ERR, "Could not create new session.");
				exit(-1);
			}
			break;
		default:
			exit(-1);
	}

	signal(SIGTERM, harakiri);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGINT, harakiri);
	signal(SIGHUP, harakiri);
		
	ret = stat(PIDFILE, buf);
	if(ret != -1) {
		llog(LOG_ERR, "PID file exists (%s), loggeragent running?.", PIDFILE);
		harakiri(0);
	}
		
	stream = fopen(PIDFILE, "w");
	fprintf(stream, "%d", (int) getpid());
	fclose(stream);
}

void show_usage(void) {
	printf("usage: loggeragent [options]\n");
	printf(" -d \tEnable the output of debugging messages.\n");
	printf(" -h \tShow this help.\n");
	printf(" -n \tDo not daemonize, print output to stderr.\n");
	printf(" -v \tShow version and exit.\n");
	exit(0);
}

/* Terminate gracefully */
void harakiri(int i) {
	unlink(PIDFILE);
	close(server);
	llog(LOG_DEBUG, "No longer listening.");

	SSL_CTX_free(ctx);
	CRYPTO_cleanup_all_ex_data();
	ERR_free_strings();
	ERR_remove_state(0);
	EVP_cleanup();
	RAND_cleanup();

	free_jobs();

	unload_modules();
	llog(LOG_DEBUG, "Unloaded modules.");

	free_config();
	llog(LOG_DEBUG, "Freed configuration data.");

	llog(LOG_INFO, "Shutting down.");
	close_log();

	if(i == SIGHUP) {
		llog(LOG_DEBUG, "Restarting", NULL);
		execv(progname, progargs);
	} else {
		exit(0);
	}
	exit(0);
}
