#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "job.h"
#include "log.h"

static struct job **jobs = NULL;
static int job_count = 0;

int add_job(char *name, void (*func)(), int interval) {
	struct job	*j = NULL;

	llog(LOG_DEBUG, "---- add_job(%s) ----", name);

	jobs = realloc(jobs, sizeof(struct job *) * (job_count + 2));
	if(jobs == NULL) {
		llog(LOG_ERR, "malloc() of jobs list failed");
		return -1;
	}
	j = malloc(sizeof(struct job));
	if(j == NULL) {
		llog(LOG_ERR, "malloc() of job failed");
		return -1;
	}
	j->name		= (char *) strdup(name);
	j->func		= func;
	j->lastrun	= time(NULL);
	j->interval	= interval;

	jobs[job_count] = j;
	jobs[job_count + 1] = NULL;

	job_count++;
	return 1;
}

void run_jobs(void) {
	struct job *j = NULL;
	int i = 0;
	int diff = 0;

	if(jobs == NULL) {
		llog(LOG_DEBUG, "jobs is NULL, skipping");
		return;
	}
	
	llog(LOG_DEBUG, "---- run_jobs() ----");
	for(i = 0; (j = jobs[i]) != NULL; i++) {
		if(j != NULL) {
			diff = time(NULL) - j->lastrun;
			if(diff >= j->interval) {
				llog(LOG_DEBUG, "Running job '%s', diff %d is greater than interval %d.", j->name, diff, j->interval);
				(*j->func)();
				j->lastrun = time(NULL);
			}
		}
	}
}

int get_job_count(void) {
	return job_count;
}

void free_jobs(void) {
	int i = 0;
	struct job *j = NULL;

	if(jobs != NULL) {
		for(i = 0; (j = jobs[i]) != NULL; i++) {
			llog(LOG_DEBUG, "Freeing job '%s'", j->name);
			free(j->name);
			free(j);
		}
		free(jobs);
		jobs = NULL;
	}
}
