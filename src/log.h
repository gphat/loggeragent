#include <syslog.h>

#define TO_DEBUG  1
#define TO_STDERR 2

void init_log(int);

void llog(int, char *, ...);

void buffer_errors(int);

char *get_error_strings(void);

void add_output(char *);

void vadd_output(char *, ...);

char *get_output(void);

void clear_output(void);

void close_log(void);
