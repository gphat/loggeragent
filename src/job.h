struct job {
	char 	*name;
	void 	(*func)();
	time_t	lastrun;
	int		interval;
};

int add_job(char *, void (*)(), int);

void run_jobs(void);

void free_jobs(void);
