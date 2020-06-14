#include "ptrace_helpers.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/user.h>

// ptrace helper functions
struct user_regs_struct get_regs(pid_t child_pid, struct user_regs_struct registers) {                                                                                
	                                                                                                                                                         
	int ptrace_result = ptrace(PTRACE_GETREGS, child_pid, 0, &registers);                                                                              
    if (ptrace_result == -1) {                                                                              
        fprintf(stderr, "dragonfly> Error (%d) during ", errno);                                                                              
        perror("ptrace");                                                                              
        exit(errno);                                                                              
    }

    return registers;                                                                              
}

void set_regs(pid_t child_pid, struct user_regs_struct registers) {

    int ptrace_result = ptrace(PTRACE_SETREGS, child_pid, 0, &registers);
    if (ptrace_result == -1) {
        fprintf(stderr, "dragonfly> Error (%d) during ", errno);
        perror("ptrace");
        exit(errno);
    }
}

long long unsigned get_value(pid_t child_pid, long long unsigned address) {
	
	errno = 0;
    long long unsigned value = ptrace(PTRACE_PEEKTEXT, child_pid, (void*)address, 0);
    if (value == -1 && errno != 0) {
        fprintf(stderr, "dragonfly> Error (%d) during ", errno);
        perror("ptrace");
        exit(errno);
    }
	
	return value;	
}

void set_breakpoint(long long unsigned bp_address, long long unsigned original_value, pid_t child_pid) {
	
	errno = 0;
    long long unsigned breakpoint = (original_value & 0xFFFFFFFFFFFFFF00 | 0xCC);
    int ptrace_result = ptrace(PTRACE_POKETEXT, child_pid, (void*)bp_address, (void*)breakpoint);
    if (ptrace_result == -1 && errno != 0) {
        fprintf(stderr, "dragonfly> Error (%d) during ", errno);
        perror("ptrace");
        exit(errno);
    }
}

void revert_breakpoint(long long unsigned bp_address, long long unsigned original_value, pid_t child_pid) {

    errno = 0;
    int ptrace_result = ptrace(PTRACE_POKETEXT, child_pid, (void*)bp_address, (void*)original_value);
    if (ptrace_result == -1 && errno != 0) {
        fprintf(stderr, "dragonfly> Error (%d) during ", errno);
        perror("ptrace");
        exit(errno);
    }
}

void resume_execution(pid_t child_pid) {

    int ptrace_result = ptrace(PTRACE_CONT, child_pid, 0, 0);
    if (ptrace_result == -1) {
        fprintf(stderr, "dragonfly> Error (%d) during ", errno);
        perror("ptrace");
        exit(errno);
    }
}
