#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

#define MAX_LOG_SIZE (5 * 1024 * 1024) // 5 MB

extern FILE *supervisor_log;

void open_supervisor_log(void);
void log_message(const char *fmt, ...); 

#endif 
