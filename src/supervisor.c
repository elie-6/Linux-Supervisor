#include "supervisor.h"
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "cgroup.h"


static int running = 1;
static program_runtime_t runtime[MAX_PROGRAMS];

// signal handler
static void handle_signal(int sig) {
    (void)sig;
    running = 0;
}

// timestamp helper
static void timestamp(char *buf, size_t len) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, len, "%Y-%m-%d %H:%M:%S", t);
}

static const char* state_to_str(program_state_t s) {
    switch(s) {
        case STATE_STOPPED: return "STOPPED";
        case STATE_STARTING: return "STARTING";
        case STATE_RUNNING: return "RUNNING";
        case STATE_EXITED: return "EXITED";
        case STATE_FAILED: return "FAILED";
        case STATE_KILLED: return "KILLED";
        default: return "UNKNOWN";
    }
}


static const char* sig_to_str(int sig) {
    switch(sig) {
        case SIGINT: return "SIGINT";
        case SIGTERM: return "SIGTERM";
        case SIGKILL: return "SIGKILL";
        case SIGHUP: return "SIGHUP";
        case SIGQUIT: return "SIGQUIT";
        default: return "UNKNOWN";
    }
}

// shutdown all children gracefully
static void shutdown_children(supervisor_config_t *config, int timeout_sec) {
    char ts[64];

    printf("\nStarting Shutdown! This might take a few seconds.\n");
    log_message("\nStarting Shutdown! This might take a few seconds.\n");

    for (size_t i = 0; i < config->count; i++) {
        if (runtime[i].pid > 0) {
            timestamp(ts, sizeof(ts));
            printf("[%s] Sending SIGTERM to %s (PID %d, state=%s)\n",
                   ts, config->programs[i].name, runtime[i].pid, state_to_str(runtime[i].state));
            kill(-runtime[i].pid, SIGTERM); 
        }
    }

    time_t start = time(NULL);
    int remaining = config->count;

    while (remaining > 0 && (time(NULL) - start) < timeout_sec) {
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            for (size_t i = 0; i < config->count; i++) {
                if (runtime[i].pid == pid) {
                    program_config_t *p = &config->programs[i];
                    timestamp(ts, sizeof(ts));

                    int exit_status;
                    if (WIFEXITED(status))
                        exit_status = WEXITSTATUS(status);
                    else if (WIFSIGNALED(status)) {
                        int sig = WTERMSIG(status);
                        exit_status = -sig;
                        runtime[i].state = STATE_KILLED;
                        printf("[%s] %s (PID %d, state=%s) killed by %s\n",
                               ts, p->name, pid, state_to_str(runtime[i].state), sig_to_str(sig));
                        log_message(" %s (PID %d, state=%s) killed by %s\n",
                                p->name, pid, state_to_str(runtime[i].state), sig_to_str(sig));
                        runtime[i].pid = 0;
                        remaining--;
                        continue;
                    } 
                    else
                        exit_status = -1;

                    runtime[i].state = (exit_status == 0) ? STATE_EXITED : STATE_FAILED;
                    printf("[%s] %s (PID %d, state=%s) exited with %d\n",
                           ts, p->name, pid, state_to_str(runtime[i].state), exit_status);
                    log_message(" %s (PID %d, state=%s) exited with %d\n",
                            p->name, pid, state_to_str(runtime[i].state), exit_status);
                    runtime[i].pid = 0;
                    remaining--;
                }
            }
        }
        usleep(100000);
    }

    for (size_t i = 0; i < config->count; i++) {
        if (runtime[i].pid > 0) {
            timestamp(ts, sizeof(ts));
            runtime[i].state = STATE_KILLED;
            printf("[%s] %s (PID %d, state=%s) did not exit, sending SIGKILL\n",
                   ts, config->programs[i].name, runtime[i].pid, state_to_str(runtime[i].state));
            log_message(" %s (PID %d, state=%s) did not exit, sending SIGKILL\n",
                    config->programs[i].name, runtime[i].pid, state_to_str(runtime[i].state));
            kill(-runtime[i].pid, SIGKILL);
            waitpid(runtime[i].pid, NULL, 0);
            runtime[i].pid = 0;
        }
    }

    printf("All children terminated, exiting supervisor.\n");
    log_message("All children terminated, exiting supervisor.\n");
}

// fork + exec a single program
static void spawn_program(program_config_t *p, program_runtime_t *r) {
    pid_t pid = fork();
    if(pid < 0) {
        perror("fork failed");
        return;
    }

    if(pid == 0) { // child
        setpgid(0, 0);

        int fd_out = -1;
        int fd_err = -1;

        if(p->stdout_path[0]) {
            fd_out = open(p->stdout_path, O_CREAT | O_WRONLY | O_APPEND, 0644);
            if(fd_out < 0) perror("open stdout");
        }

        if(p->stderr_path[0]) {
            if(fd_out >= 0 && strcmp(p->stdout_path, p->stderr_path) == 0) {
                fd_err = fd_out;
            } else {
                fd_err = open(p->stderr_path, O_CREAT | O_WRONLY | O_APPEND, 0644);
                if(fd_err < 0) perror("open stderr");
            }
        }

        if(fd_out >= 0) { dup2(fd_out, STDOUT_FILENO); if(fd_out != fd_err) close(fd_out); }
        if(fd_err >= 0) { dup2(fd_err, STDERR_FILENO); if(fd_err != fd_out) close(fd_err); }

        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);

        execl("/bin/sh", "sh", "-c", p->command, NULL);
        perror("exec failed");
        exit(1);
    }
    else { // parent
         r->pid = pid;
        // apply cgroup limits
        if (p->memory_limit_bytes > 0 || p->cpu_limit > 0) {
            if (cgroup_setup(p, pid) != 0) {
                char ts[64];
                timestamp(ts, sizeof(ts));
                printf("[%s] Failed to apply cgroup for %s (PID %d)\n", ts, p->name, pid);
            }
        }
        r->state = STATE_RUNNING;
        char ts[64];
        timestamp(ts, sizeof(ts));
        printf("[%s] Spawned %s (PID %d, state=%s)\n", ts, p->name, pid, state_to_str(r->state));
        log_message("Spawned %s (PID %d, state=%s)\n", p->name, pid, state_to_str(r->state));
    }
}

// main supervisor loop
void supervisor_run(supervisor_config_t *config) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("\nStarting Supervisor ... \n");
    log_message("\nStarting Supervisor ... \n");

    for(size_t i = 0; i < config->count; i++) {
        runtime[i].pid = 0;
        runtime[i].restart_count = 0;
        runtime[i].state = STATE_STOPPED;
    }

    for(size_t i = 0; i < config->count; i++) {
        if(config->programs[i].autostart)
            spawn_program(&config->programs[i], &runtime[i]);
    }

    while(running) {
        int status;
        pid_t pid;

        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            for(size_t i = 0; i < config->count; i++) {
                if(runtime[i].pid == pid) {
                    program_config_t *p = &config->programs[i];

                    char ts[64];
                    timestamp(ts, sizeof(ts));

                    int exit_status;
                    if (WIFEXITED(status))
                        exit_status = WEXITSTATUS(status);
                    else if (WIFSIGNALED(status)) {
                        int sig = WTERMSIG(status);
                        exit_status = -sig;
                        runtime[i].state = STATE_KILLED;
                        printf("[%s] %s (PID %d, state=%s) killed by %s\n",
                               ts, p->name, pid, state_to_str(runtime[i].state), sig_to_str(sig));
                        log_message(" %s (PID %d, state=%s) killed by %s\n",
                                p->name, pid, state_to_str(runtime[i].state), sig_to_str(sig));
                        runtime[i].pid = 0;
                        runtime[i].restart_count = 0;
                        continue;
                    } else
                        exit_status = -1;

                    runtime[i].state = (exit_status == 0) ? STATE_EXITED : STATE_FAILED;
                    printf("[%s] %s (PID %d, state=%s) exited with %d\n",
                           ts, p->name, pid, state_to_str(runtime[i].state), exit_status);
                    log_message(" %s (PID %d, state=%s) exited with %d\n",
                            p->name, pid, state_to_str(runtime[i].state), exit_status);

                    int restart = 0;

                    if(p->autorestart == RESTART_ALWAYS) {
                        restart = 1;
                    } else if(p->autorestart == RESTART_ON_FAILURE && exit_status != 0) {
                        if(p->max_restarts == 0 || runtime[i].restart_count < p->max_restarts) {
                            restart = 1;
                            runtime[i].restart_count++;
                        }
                    }

                    if(restart) {
                        timestamp(ts, sizeof(ts));
                        if(p->autorestart == RESTART_ON_FAILURE)
                            log_message(" Restarting %s (%d/%d)\n", p->name,
                                runtime[i].restart_count,
                                p->max_restarts == 0 ? -1 : p->max_restarts);
                        else {
                            printf("[%s] Restarting %s\n", ts, p->name);
                            log_message(" Restarting %s\n", p->name);
                        }

                        if(p->restart_delay > 0) sleep(p->restart_delay);
                        spawn_program(p, &runtime[i]);
                    } else {
                        runtime[i].pid = 0;
                        runtime[i].state = STATE_STOPPED;
                        if(p->autorestart == RESTART_ON_FAILURE && exit_status != 0 &&
                           p->max_restarts != 0 && runtime[i].restart_count >= p->max_restarts) {
                            printf("[%s] %s reached max restarts (%d), not restarting\n",
                                   ts, p->name, p->max_restarts);
                            log_message(" %s reached max restarts (%d), not restarting\n",
                                    p->name, p->max_restarts);
                        }
                    }
                }
            }
        }

        usleep(100000);
    }

    shutdown_children(config, 3);

    for (size_t i = 0; i < config->count; i++) 
    {
        cgroup_cleanup(config->programs[i].name);
    }
     

}