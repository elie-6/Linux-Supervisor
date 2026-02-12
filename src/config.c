#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LINE_LEN 512

// trim leading/trailing whitespace
static void trim(char *str) {
    // leading
    char *start = str;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (start != str)
        memmove(str, start, strlen(start) + 1);

    // trailing
    char *end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
}


static int parse_bool(const char *value, bool *out) {
    if (strcasecmp(value, "true") == 0) { *out = true; return 0; }
    if (strcasecmp(value, "false") == 0) { *out = false; return 0; }
    return -1;
}


static int parse_restart_policy(const char *value, restart_policy_t *out) {
    if (strcasecmp(value, "never") == 0) { *out = RESTART_NEVER; return 0; }
    if (strcasecmp(value, "on-failure") == 0) { *out = RESTART_ON_FAILURE; return 0; }
    if (strcasecmp(value, "always") == 0) { *out = RESTART_ALWAYS; return 0; }
    return -1;
}

//memory parser
int parse_memory(const char *value, long *result) {
    char unit[3] = {0};
    long number;

    if (sscanf(value, "%ld%2s", &number, unit) < 1)
        return -1;

    if (number < 0)
        return -1;

    if (strcasecmp(unit, "GB") == 0)
        *result = number * 1024L * 1024L * 1024L;
    else if (strcasecmp(unit, "MB") == 0)
        *result = number * 1024L * 1024L;
    else if (strcasecmp(unit, "KB") == 0)
        *result = number * 1024L;
    else if (unit[0] == '\0')  // raw bytes
        *result = number;
    else
        return -1;

    return 0;
}
//cpu input
int parse_cpu(const char *value, double *result) {
    char *endptr;
    double val = strtod(value, &endptr);

    if (*endptr != '\0')
        return -1;

    if (val < 0.0)
        return -1;

    *result = val;
    return 0;
}



// load config
int load_config(const char *filename, supervisor_config_t *config) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Failed to open config file");
        return -1;
    }

    char line[LINE_LEN];
    program_config_t *current = NULL;
    size_t line_number = 0;
    int in_program = 0;

    config->count = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_number++;
        trim(line);

        // skip blank lines and comments
        if (line[0] == '\0' || line[0] == '#')
            continue;

        // program block
        if (strncmp(line, "program ", 8) == 0) {
            if (config->count >= MAX_PROGRAMS) {
                fprintf(stderr, "Line %zu: too many programs\n", line_number);
                fclose(fp);
                return -1;
            }

            current = &config->programs[config->count++];
            memset(current, 0, sizeof(program_config_t));

            char *name = line + 8;
            trim(name);
            if (strlen(name) == 0) {
                fprintf(stderr, "Line %zu: program name missing\n", line_number);
                fclose(fp);
                return -1;
            }
            strncpy(current->name, name, MAX_NAME_LEN - 1);

            // defaults
            current->autostart = true;
            current->autorestart = RESTART_NEVER;
            current->restart_delay = 0;
            current->max_restarts = 0;

            in_program = 1;
            continue;
        }

  
        if (!in_program) {
            fprintf(stderr, "Line %zu: key=value outside program block\n", line_number);
            fclose(fp);
            return -1;
        }

        // parse key=value
        char *eq = strchr(line, '=');
        if (!eq) {
            fprintf(stderr, "Line %zu: invalid line (no =)\n", line_number);
            fclose(fp);
            return -1;
        }
        *eq = '\0';
        char *key = line;
        char *value = eq + 1;
        trim(key);
        trim(value);

        if (strcasecmp(key, "command") == 0) {
            strncpy(current->command, value, MAX_COMMAND_LEN - 1);
        } else if (strcasecmp(key, "autostart") == 0) {
            if (parse_bool(value, &current->autostart) != 0) {
                fprintf(stderr, "Line %zu: invalid boolean for autostart\n", line_number);
                fclose(fp);
                return -1;
            }
        } else if (strcasecmp(key, "autorestart") == 0) {
            if (parse_restart_policy(value, &current->autorestart) != 0) {
                fprintf(stderr, "Line %zu: invalid restart policy\n", line_number);
                fclose(fp);
                return -1;
            }
        } else if (strcasecmp(key, "restart_delay") == 0) {
            current->restart_delay = atoi(value);
        } else if (strcasecmp(key, "max_restarts") == 0) {
            current->max_restarts = atoi(value);
        } else if (strcasecmp(key, "stdout") == 0) {
            strncpy(current->stdout_path, value, MAX_PATH_LEN - 1);
        } else if (strcasecmp(key, "stderr") == 0) {
            strncpy(current->stderr_path, value, MAX_PATH_LEN - 1);
                    } else if (strcasecmp(key, "memory_limit") == 0) {
            if (parse_memory(value, &current->memory_limit_bytes) != 0) {
                fprintf(stderr, "Line %zu: invalid memory_limit\n", line_number);
                fclose(fp);
                return -1;
            }

        } else if (strcasecmp(key, "cpu_limit") == 0) {
            if (parse_cpu(value, &current->cpu_limit) != 0) {
                fprintf(stderr, "Line %zu: invalid cpu_limit\n", line_number);
                fclose(fp);
                return -1;
            }
        } else {
            fprintf(stderr, "Line %zu: unknown key '%s'\n", line_number, key);
            fclose(fp);
            return -1;
        }
    }

    // sohet l saleme field check
    for (size_t i = 0; i < config->count; i++) {
        if (strlen(config->programs[i].command) == 0) {
            fprintf(stderr, "Program '%s' missing command\n", config->programs[i].name);
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}
