#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_PROGRAMS 64
#define MAX_NAME_LEN 64
#define MAX_COMMAND_LEN 256
#define MAX_PATH_LEN 256

// restart policy enum
typedef enum {
    RESTART_NEVER,
    RESTART_ON_FAILURE,
    RESTART_ALWAYS
} restart_policy_t;

// structure for program config
typedef struct {
    char name[MAX_NAME_LEN];
    char command[MAX_COMMAND_LEN];
    bool autostart;
    restart_policy_t autorestart;
    int restart_delay;       // seconds
    int max_restarts;
    long memory_limit_bytes;   //MB
    double cpu_limit;      //0<x<1
    char stdout_path[MAX_PATH_LEN];
    char stderr_path[MAX_PATH_LEN];
} program_config_t;

// structure for entire config file
typedef struct {
    program_config_t programs[MAX_PROGRAMS];
    size_t count;
} supervisor_config_t;

// Parser API
int load_config(const char *filename, supervisor_config_t *config);

#endif 
