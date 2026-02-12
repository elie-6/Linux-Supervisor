#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>

FILE *supervisor_log = NULL;

// open the supervisor log, rotate if big
void open_supervisor_log(void) {
    if (supervisor_log) {
        fclose(supervisor_log);
        supervisor_log = NULL;
    }

    struct stat st;
    if (stat("supervisor.log", &st) == 0 && st.st_size >= MAX_LOG_SIZE) {
        char backup[256];
        time_t now = time(NULL);
        strftime(backup, sizeof(backup), "supervisor-%Y%m%d-%H%M%S.log", localtime(&now));
        if (rename("supervisor.log", backup) != 0) {
            perror("Failed to rotate supervisor.log");
        }
    }

    supervisor_log = fopen("supervisor.log", "a");
    if (!supervisor_log) {
        perror("Failed to open supervisor.log");
        supervisor_log = stdout; // fallback to stdout
    }
}


void log_message(const char *fmt, ...) {
    if (!supervisor_log) open_supervisor_log();

    char ts[64];
    time_t now = time(NULL);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(supervisor_log, "[%s] ", ts);

    va_list args;
    va_start(args, fmt);
    vfprintf(supervisor_log, fmt, args);
    va_end(args);

    fprintf(supervisor_log, "\n");
    fflush(supervisor_log); // written immediately
}
