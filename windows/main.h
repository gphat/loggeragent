void serviceMain(int, char**);
void controlHandler(DWORD);
int initService();
void installService();
void removeService();
int writeToLog(char*);
void lstats_free(struct s_logger_stats);
