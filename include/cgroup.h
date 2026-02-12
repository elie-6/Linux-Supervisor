#ifndef CGROUP_H
#define CGROUP_H

#include <sys/types.h>
#include "config.h"

int cgroup_setup(program_config_t *p, pid_t pid);
void cgroup_cleanup(const char *name);

#endif
