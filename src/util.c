#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "log.h"

char *error = NULL;

char *lprintf(char *fmt, ...) {
	va_list ap;
	unsigned long len;
	char *str = NULL;

	str = malloc(sizeof(char) * 2);
	if(str == NULL) {
		llog(LOG_ERR, "Could not allocate initial lprintf() buffer.");
		return NULL;
	}

	va_start(ap, fmt);

	len = vsnprintf(str, 1, fmt, ap);
	if(len == -1) {
		llog(LOG_ERR, "Error in vsnprintf().");
		return NULL;
	}

	str = (char *) realloc(str, (sizeof(char) * (len + 2)));

	if(str == NULL) {
		llog(LOG_ERR, "Error allocating lprintf() buffer.");
		return NULL;
	}

	len = vsnprintf(str, len + 1, fmt, ap);
	if(len == -1) {
		llog(LOG_ERR, "Error in second vsnprintf().");
		return NULL;
	}

	va_end(ap);

	return str;
}

/* This function modifies the pointed-to-string directly, and simply
 * turns any trailing \n's or \r's into \0's.  This causes you to waste a byte for
 * every trailing \n replaced.
 */
int chomp(char *cstr) {
	int offset, count = 0;

	if(cstr == NULL) {
		return 0;
	}

	offset = strlen(cstr);
	if(offset < 1) {
		return 0;
	}

	/* Trim off the NULL byte. */
	offset--;
	for(; offset >= 0; offset--) {
		if((cstr[offset] == '\n') || (cstr[offset] == '\r')) {
			cstr[offset] = '\0';
			count++;
		} else {
			break;
		}
	}

	return count;
}

/* This function modifies the pointed-to-string directly, and simply
 * turns any trailing whitespace into \0's or increments the pointer to skip
 * any leading whitespace.  This causes you to waste a byte for every byte
 * of whitespace you skip.
 */
int trim(char *tstr) {
	int count = 0;
	int offset = 0;

	if(tstr == NULL) {
		return;
	}

	offset = strlen(tstr);
	offset--;
	for(; offset >= 0; offset--) {
		if(isspace(tstr[offset])) {
			tstr[offset] = '\0';		
			count++;
		} else {
			break;
		}
	}

	return count;
}

/* Function used to add entropy to OpenSSL's PRNG.  I know it's predictable.
 * Send a patch. ;)
 */
char *gen_random(int size) {
	int i = 0;
	time_t now;
	char rand;
	char *buf;

	buf = malloc(sizeof(char) * size);
	if(buf == NULL) {
		llog(LOG_ERR, "Could not malloc() random buffer.");
		return NULL;
	}

	if(size <= 0) {
		return NULL;
	}

	srandom(time(&now));

	for(i = 0; i < size; i++) {
		rand = random();
		buf[i] = rand;
	}

	return buf;
}
