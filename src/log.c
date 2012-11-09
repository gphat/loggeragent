#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "log.h"

static int debug = 0;
static int copystderr = 0;
static int buffererr = 0;
static char *errors = NULL;
static int have_output = 0;
static char *output = NULL;

static void add_error(char *);

void init_log(int d) {
	if(d & TO_DEBUG) {
		debug = 1;
	}
	if(d & TO_STDERR) {
		copystderr = 1;
	}
	openlog("loggeragent", LOG_NDELAY | LOG_PID, LOG_DAEMON);
}

void llog(int severity, char *format, ...) {
	va_list ap;
	int len;
	char *msg = NULL;

	if(format == NULL) {
		llog(LOG_ERR, "NULL format to llog()?");
		return;
	}

	/* Take off all the \r's and \n's */
	chomp(format);

	msg = (char *) malloc(sizeof(char) * 2);
	if(msg == NULL) {
		printf("Could not malloc() initial log buffer.");
		return;
	}

	va_start(ap, format);

	len = vsnprintf(msg, 1, format, ap);
	if(len < 0) {
		free(msg);
		printf("vsnprintf() failed, returned %d: %s", len, format);
		return;
	}

	msg = (char *) realloc(msg, (sizeof(char) * (len + 2)));
	if(msg == NULL) {
		printf("Could not malloc() log buffer.");
		return;
	}

	len = vsnprintf(msg, (size_t) len + 1, format, ap);
	if(len < 0) {
		free(msg);
		printf("vsnprintf() #2 failed, returned %d: %s", len, format);
		return;
	}

	va_end(ap);

	if(severity == LOG_ERR) {
		if(buffererr != 0) {
			add_error(msg);
		}
	}
	if(severity != LOG_DEBUG) {
		syslog(severity, msg);
		if(copystderr != 0) {
			fprintf(stderr, "%s\n", msg);
		}
	} else {
		if(debug != 0) {
			syslog(severity, msg);
			if(copystderr || (severity == LOG_ERR)) {
				fprintf(stderr, "%s\n", msg);
			}
		}
	}
	free(msg);
	return;
}

void buffer_errors(int i) {
	buffererr = i;
	if(buffererr == 0) {
		llog(LOG_DEBUG, "No longer buffering errors.");
		if(errors != NULL) {
			free(errors);
			llog(LOG_DEBUG, "free()ed errors");
		}
		errors = NULL;
	} else {
		llog(LOG_DEBUG, "Buffering errors.");
	}
}

static void add_error(char *e) {
	size_t elen = 0, curlen = 0;

	if(e == NULL) {
		return;
	}
	elen = strlen(e);
	if(elen == 0) {
		return;
	}
	if(errors != NULL) {
		curlen = strlen(errors);
	}
	llog(LOG_DEBUG, "Adding error '%s'", e);

	if(errors == NULL) {
		errors = (char *) malloc(sizeof(char) * (elen + 1));
		if(errors == NULL) {
			llog(LOG_ERR, "Couldn't allocate string for error holding.");
			return;
		}
		memset(errors, 0, (elen + 1));
		strncpy(errors, e, elen);
	} else {
		errors = (char *) realloc(errors, (curlen + elen + 2));
		if(errors == NULL) {
			llog(LOG_ERR, "Couldn't reallocate error holding var.");
		}
		if(snprintf(errors, (elen + 1), "%s\n", e) < 0) {
			llog(LOG_ERR, "Error adding new error to error buffer.");
		}
	}
}

char *get_error_strings() {
	return errors;
}

void add_output(char *out) {
	size_t outsize = 0, cursize = 0;

	if(out == NULL) {
		return;
	}

	outsize = strlen(out);

	if(output == NULL) {
		output = malloc(sizeof(char) * (outsize + 1));
		if(output == NULL) {
			llog(LOG_ERR, "malloc() of output buffer failed.");
			return;
		}
		memset(output, 0, (outsize + 1));
		strncpy(output, out, outsize);
	} else {
		cursize = strlen(output);
		output = realloc(output, sizeof(char) * (outsize + cursize + 1));
		if(output == NULL) {
			llog(LOG_ERR, "realloc() of output buffer failed.");
			return;
		}
		strncat(output, out, cursize);
	}
}

void vadd_output(char *format, ...) {
	va_list ap;
	int len;
	char *msg = NULL;

	if(format == NULL) {
		llog(LOG_ERR, "NULL format to vadd_output()");
		return;
	}

	chomp(format);

	if((msg = malloc(sizeof(char) * 2)) == NULL) {
		llog(LOG_ERR, "Can't malloc() log string.");
		return;
	}

	va_start(ap, format);

	len = vsnprintf(msg, 1, format, ap);
	if(len < 0) {
		free(msg);
		printf("vsnprintf() failed, returned %d: %s", len, format);
		return;
	}

	msg = (char *) realloc(msg, (sizeof(char) * (len + 2)));
	if(msg == NULL) {
		printf("Could not malloc() output buffer.");
		return;
	}

	len = vsnprintf(msg, (size_t) len + 1, format, ap);
	if(len , 0) {
		free(msg);
		llog(LOG_ERR, "vadd_output vsnprintf #2 failed, returned %d: %s", len, format);
		return;
	}

	va_end(ap);
	add_output(msg);
	free(msg);
}

char *get_output() {
	return output;
}

void clear_output() {
	free(output);
	output = NULL;
}

void close_log() {
	if(errors != NULL) {
		free(errors);
	}
	closelog();
}
