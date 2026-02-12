# Linux Process Supervisor

A **minimal, fully functional Linux process supervisor** in C, implementing robust process lifecycle management, resource enforcement, and logging.

Built to demonstrate low-level Linux system programming, cgroup-based resource limits, and reliable process supervision.

---

## Architecture
```
+-------------------+
|   Supervisor      |  <- Single C binary (Msupervisor)
|  (load config,    |
| manage programs)  |
+--------+----------+
         |
         v
+-------------------+
|  Program Registry |  <- Tracks runtime state
| (PID, state, logs)|
+--------+----------+
         |
         v
+-------------------+
| Lifecycle Manager |  <- fork/exec, waitpid loop
|(start/stop/restart)
+--------+----------+
         |
         v
+-------------------+
| Resource Enforcer |  <- cgroup memory + CPU limits
| (kill/restart)    |
+-------------------+
         |
         v
+-------------------+
|       Logger      |  <- stdout/stderr + state transitions
+-------------------+
```

---

## Overview

This supervisor manages multiple programs via a configuration file while enforcing CPU and memory limits using Linux cgroups. It guarantees reliable restarts, precise resource enforcement, and detailed logging.

**Key capabilities**:

- Per-program **autostart** and **autorestart** policies (`never`, `on-failure`, `always`)
- Enforces **memory** and **CPU limits** using Linux cgroups
- Logs stdout/stderr to configurable files
- Graceful **signal handling** for shutdown
- Monitoring & restart logic implemented in a **blocking waitpid loop**
- Fully tested with memory-hogging processes

---

## Tech Stack

**Language**: C (GCC)  
**Build**: Makefile  
**OS**: Linux-only (cgroup v2, POSIX APIs)  

**Core Components**:
- `src/main.c` — entry point, loads config, starts supervisor loop
- `src/supervisor.c` — main lifecycle manager, blocking wait loop, restart logic, graceful shutdown
- `src/config.c` — config parser, program struct population
- `src/cgroup.c` — memory and CPU enforcement using Linux cgroups
- `src/logging.c` — stdout/stderr redirection, log rotation hooks

**Supporting scripts**:
- `supervisor.conf` — configuration file for programs, limits, and logging
- `Makefile` - Easy and Fast; Build, Run, Clean

---

## Project Structure
```
process-supervisor/
├─ src/
│  ├─ main.c
│  ├─ supervisor.c
│  ├─ config.c
│  ├─ logging.c
│  └─ cgroup.c
│  
├─ include/                  # header files
├─ supervisor.conf           # example config
├─ logs/                     # runtime logs
├─ Makefile
└─ README.md
```

---

## Configuration Example
```ini
program web
command=/usr/bin/python3 -m http.server 8080
autostart=true
autorestart=on-failure
restart_delay=2
max_restarts=5
memory_limit=500KB
cpu_limit=0.5
stdout=logs/web.log
stderr=logs/web.log

program memhog
command=/home/user/memhog.sh
autostart=true
autorestart=on-failure
restart_delay=0
max_restarts=0
memory_limit=30MB
cpu_limit=0.5
stdout=logs/memhog.log
stderr=logs/memhog.log
```

---

## Build & Run

### Build
```bash
# Clean and build
make clean
make
```

### Run
```bash
make run
```

- Logs appear in `supervisor.log` as automatically configured at first run in root
- State transitions and resource kills + stderr appear in `logs/`

---

## Resource Enforcement (Cgroups)

Each program gets its own cgroup at `/sys/fs/cgroup/supervisor/<program_name>/`

- **Memory limits** trigger kill-and-restart if exceeded
- **CPU limits** throttle program execution based on `cpu.cfs_quota_us` / `cpu.cfs_period_us`
- Supervisor monitors resource usage live, enforcing policies reliably

---

## Testing Memory Limits

### Bash / Perl Example
```bash
#!/bin/bash
# memhog.sh
while true; do
  perl -e 'my $a = "a" x 1048576; sleep 0.01'
done
```

Use `memhog.sh` in your config to stress memory limits.

**Monitor live**:
```bash
watch -n1 cat /sys/fs/cgroup/supervisor/memhog/memory.usage_in_bytes
```

### C Example (memhog.c)
```c
#include <stdlib.h>
#include <unistd.h>

int main() {
    while (1) {
        void *p = malloc(1024*1024); // 1MB blocks
        if (!p) break;
        usleep(1000);                // throttle allocation slightly
    }
    return 0;
}
```

Compiled binary triggers memory limits quickly for supervisor testing.

---

## Observing Resource Limits

- **Memory usage**: `/sys/fs/cgroup/supervisor/<program>/memory.usage_in_bytes`
- **CPU usage**: `/sys/fs/cgroup/supervisor/<program>/cpuacct.usage`
- **Logs** show program state: `RUNNING`, `FAILED`, `KILLED`

---

## Design Highlights

**Supervisor loop**: Simple, deterministic process supervision.

**Automatic restarts**: Configurable max attempts and delays.

**Cgroup integration**: Guarantees memory/CPU enforcement. (Configurable too)

**Logging**: Every state transition, resource violation, and program output captured.

---

## Why This Matters

This project demonstrates:

- **Linux process lifecycle management**
- **Reliable enforcement of resource limits via cgroups**
- **Supervisor patterns akin to systemd / supervisord**
- **Low-level OS concepts**: fork/exec, waitpid, signals, cgroups

It's fully working, tested, and production-grade in the scope of a project — and demonstrates my Linux expertise.

**Developed on Ubuntu Linux VM accessed via SSH from MacOS**

---
