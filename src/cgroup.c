#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include "cgroup.h"

#define CGROUP_ROOT "/sys/fs/cgroup"
#define SUPERVISOR_GROUP "supervisor"


static void ensure_dir(const char *path) {
    if (mkdir(path, 0755) != 0) {
        if (errno != EEXIST) {
            perror("mkdir failed");
        }
    }
}


static void write_file(const char *path, const char *value) {
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("fopen failed");
        return;
    }

    fprintf(f, "%s", value);
    fclose(f);
}


static void apply_cpu_limit(const char *program_path, double cpu_limit) {
    if (cpu_limit <= 0) return;

    char cpu_file[256];
    snprintf(cpu_file, sizeof(cpu_file),
             "%s/cpu.max", program_path);

    long quota = (long)(cpu_limit * 100000); // 0.5 CPU to 50000
    char value[64];
    snprintf(value, sizeof(value), "%ld 100000", quota);

    write_file(cpu_file, value);
}


static void apply_memory_limit(const char *program_path, long memory_bytes) {
    if (memory_bytes <= 0) return;

    char mem_file[256];
    snprintf(mem_file, sizeof(mem_file),
             "%s/memory.max", program_path);

    char value[64];
    snprintf(value, sizeof(value), "%ld", memory_bytes);

    write_file(mem_file, value);
}


 // main setup function.

int cgroup_setup(program_config_t *p, pid_t pid) {

    char supervisor_path[512];
    char program_path[512];

 
    snprintf(supervisor_path, sizeof(supervisor_path), "%s/%s", CGROUP_ROOT, SUPERVISOR_GROUP);
    ensure_dir(supervisor_path);

    int n = snprintf(program_path, sizeof(program_path),"%s/%s", supervisor_path, p->name);
    if (n < 0 || n >= (int)sizeof(program_path))
    {
        fprintf(stderr, "Program name too long for cgroup path: %s\n", p->name);
        return -1;
    }
    ensure_dir(program_path);

    // apply resource limits
    apply_memory_limit(program_path, p->memory_limit_bytes);
    apply_cpu_limit(program_path, p->cpu_limit);

    // add process to cgroup
    char procs_file[512];
    n = snprintf(procs_file, sizeof(procs_file),"%s/cgroup.procs", program_path);
    if (n < 0 || n >= (int)sizeof(procs_file)) {
        fprintf(stderr, "Cgroup procs path too long for program: %s\n", p->name);
        return -1;
    }

    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);

    write_file(procs_file, pid_str);

    return 0;
}


void cgroup_cleanup(const char *name) {

    char path[256];
    snprintf(path, sizeof(path),
             "%s/%s/%s",
             CGROUP_ROOT,
             SUPERVISOR_GROUP,
             name);

    if (rmdir(path) != 0) {
        perror("rmdir failed");
    }
}
