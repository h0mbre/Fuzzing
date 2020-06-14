#include <sys/types.h>
#include <unistd.h>
#include <sys/user.h>

struct user_regs_struct get_regs(pid_t child_pid, struct user_regs_struct registers);

void set_regs(pid_t child_pid, struct user_regs_struct registers);

long long unsigned get_value(pid_t child_pid, long long unsigned address);

void set_breakpoint(long long unsigned bp_address, long long unsigned original_value, pid_t child_pid);

void revert_breakpoint(long long unsigned bp_address, long long unsigned original_value, pid_t child_pid);

void resume_execution(pid_t child_pid);
