#include "config.h"
#include "supervisor.h"
#include <stdio.h>

const char *restart_policy_str(restart_policy_t p) {
    switch (p) {
        case RESTART_NEVER: return "never";
        case RESTART_ON_FAILURE: return "on-failure";
        case RESTART_ALWAYS: return "always";
        default: return "unknown";
    }
}

int main(int argc, char *argv[]) {
    const char *config_file = "supervisor.conf";
    if (argc > 1) {
        config_file = argv[1];
    }

    supervisor_config_t config;
    if (load_config(config_file, &config) != 0) {
        fprintf(stderr, "Failed to load config file: %s\n", config_file);
        return 1;
    }

    printf("Loaded %zu programs from %s\n\n", config.count, config_file);

    for (size_t i = 0; i < config.count; i++) {
        program_config_t *p = &config.programs[i];
        printf("Program: %s\n", p->name);
        printf("  command: %s\n", p->command);
        printf("  autostart: %s\n", p->autostart ? "true" : "false");
        printf("  autorestart: %s\n", restart_policy_str(p->autorestart));
        printf("  restart_delay: %d\n", p->restart_delay);
        printf("  max_restarts: %d\n", p->max_restarts);
        printf("  memory_limit: %ld\n", p->memory_limit_bytes);
        printf("  cpu_limit: %f\n", p->cpu_limit);
        printf("  stdout: %s\n", p->stdout_path[0] ? p->stdout_path : "(none)");
        printf("  stderr: %s\n", p->stderr_path[0] ? p->stderr_path : "(none)");
        printf("\n");
    }

     // run supervisor
    supervisor_run(&config);

    return 0;
}
