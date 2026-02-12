#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include "config.h"
#include <unistd.h>

// program runtime states
typedef enum {
    STATE_STOPPED,    
    STATE_STARTING,   
    STATE_RUNNING,     
    STATE_EXITED,      
    STATE_FAILED,      
    STATE_KILLED       
} program_state_t;


typedef struct {
    pid_t pid;
    int restart_count;            // count restarts for ON_FAILURE only
    program_state_t state;     // current state
} program_runtime_t;


void supervisor_run(supervisor_config_t *config);

#endif 
