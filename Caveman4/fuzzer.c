// gcc dragonfly.c ptrace_helpers.c snapshot.c -o dragonfly

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <sys/types.h> 
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/personality.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/uio.h>
#include <signal.h>

#include "snapshot.h"
#include "ptrace_helpers.h"

// globals /////////////////////////////////////// 
char* debugee = "vuln";                         //
size_t prototype_count = 7958;                  //
unsigned char input_prototype[7958];            //
unsigned char input_mutated[7958];              //
void* fuzz_location = (void*)0x5555557594B0;    //
int corpus_count = 0;                           //
unsigned char *corpus[100];                     //       
//////////////////////////////////////////////////

// breakpoints ///////////////////////////////////
long long unsigned start_addr = 0x555555554b41; //
long long unsigned end_addr = 0x5555555548c0;   //
//////////////////////////////////////////////////

// dynamic breakpoints ///////////////////////////
struct dynamic_breakpoints {                    //
                                                //
    int bp_count;                               //
    long long unsigned bp_addresses[2];         //
    long long unsigned bp_original_values[2];   //
                                                //
};                                              //
struct dynamic_breakpoints vuln;                //
//////////////////////////////////////////////////

void set_dynamic_breakpoints(pid_t child_pid) {

    // these are the breakpoints that inform our code coverage
    vuln.bp_count = 2;
    vuln.bp_addresses[0] = 0x555555554b7d;
    vuln.bp_addresses[1] = 0x555555554bb9;

    for (int i = 0; i < vuln.bp_count; i++) {
        vuln.bp_original_values[i] = get_value(child_pid, vuln.bp_addresses[i]);
    }
    //printf("\033[1;35mdragonfly>\033[0m original dynamic breakpoint values collected\n");

    for (int i = 0; i < vuln.bp_count; i++) {
        set_breakpoint(vuln.bp_addresses[i], vuln.bp_original_values[i], child_pid);
    }
    printf("\033[1;35mdragonfly>\033[0m set dynamic breakpoints: \n\n");
    for (int i = 0; i < vuln.bp_count; i++) {
        printf("           0x%llx\n", vuln.bp_addresses[i]);
    }
    printf("\n");
}

void restore_dynamic_breakpoint(pid_t child_pid, long long unsigned bp_addr) {

    int reverted = 0; 
    for (int i = 0; i < vuln.bp_count; i++) {
        if (vuln.bp_addresses[i] == bp_addr) {
            revert_breakpoint(bp_addr, vuln.bp_original_values[i], child_pid);
            reverted++;
        }
    }
    if (!reverted) {
        printf("\033[1;35mdragonfly>\033[0m unable to revert breakpoint: 0x%llx\n", bp_addr);
        exit(-1);
    }
}

void add_to_corpus(unsigned char *new_input) {

    corpus_count++;
    corpus[corpus_count - 1] = new_input;

}

void add_code_coverage(pid_t child_pid, struct user_regs_struct registers) {

    // search through dynamic breakpoints for match so we can restore original value
    restore_dynamic_breakpoint(child_pid, registers.rip - 1);

    // save input in heap
    unsigned char *new_input = (unsigned char*)malloc(prototype_count);
    memcpy(new_input, input_mutated, prototype_count);
    add_to_corpus(new_input);

}

unsigned char* get_fuzzcase() {

    // default corpus, ie, the prototype input only (fast)
    if (corpus_count == 0) {
        memcpy(input_mutated, input_prototype, prototype_count);

        // mutate 159 bytes, while keeping the prototype intact
        for (int i = 0; i < (int)(prototype_count * .02); i++) {
            input_mutated[rand() % prototype_count] = (unsigned char)(rand() % 256);
        }

        return input_mutated;
    }

    // if we're here, that means our corpus has been added to
    // by code coverage feedback and we're doing lookups in heap (slow)
    else {
        
        // leave a possibility of still using prototype input 10% of the time
        int choice = rand() % 10;
        
        // if we select 0 here, use prototype instead of new inputs we got
        if (choice == 0) {
            memcpy(input_mutated, input_prototype, prototype_count);

            // mutate 159 bytes, while keeping the prototype intact
            for (int i = 0; i < (int)(prototype_count * .02); i++) {
                input_mutated[rand() % prototype_count] = (unsigned char)(rand() % 256);
            }

            return input_mutated;
        }
        // here we'll pick a random input from the corpus 
        else {
            int corpus_pick = rand() % corpus_count;
            memcpy(input_mutated, corpus[corpus_pick], prototype_count);

            for (int i = 0; i < (int)(prototype_count * .02); i++) {
                input_mutated[rand() % prototype_count] = (unsigned char)(rand() % 256);
            }

            return input_mutated;
        }
    }
}

void insert_fuzzcase(unsigned char* input_mutated, pid_t child_pid) {

    
    struct iovec local[1];
    struct iovec remote[1];
    
    local[0].iov_base = input_mutated;
    local[0].iov_len = prototype_count;
 
    remote[0].iov_base = fuzz_location;
    remote[0].iov_len = prototype_count;
 
    ssize_t bytes_written = process_vm_writev(child_pid, local, 1, remote, 1, 0);
}

void log_crash(unsigned char* crash_input, struct user_regs_struct crash_registers) {

    // name file with crash rip and signal
    char file_name[0x30];
    sprintf(file_name, "crashes/11.%llx", crash_registers.rip);

    // write to disk
    FILE *fileptr;
    fileptr = fopen(file_name, "wb");
    if (fileptr == NULL) {
        printf("\033[1;35mdragonfly>\033[0m unable to log crash data.\n");
        return;
    }

    fwrite(crash_input, 1, prototype_count, fileptr);
    fclose(fileptr);
}

void print_stats(int result, int crashes, float million_iterations) {

    double percentage = (corpus_count / vuln.bp_count) * 100;

    printf("\e[?25l\n\033[1;36mfc/s\033[0m       : %d\n\033[1;36mcrashes\033[0m"
    "    : %d\n\033[1;36miterations\033[0m : %0.1fm"
    "\n\033[1;36mcoverage\033[0m   : %d/%d (%%%0.2f)"
    "\033[F\033[F\033[F\033[F",
    result, crashes, million_iterations, corpus_count, vuln.bp_count, percentage);
    fflush(stdout);
}

void fuzzer(pid_t child_pid, unsigned char* snapshot_buf, struct user_regs_struct snapshot_registers) {

    // fc/s
    int iterations = 0;
    int crashes = 0;
    clock_t t;
    t = clock();

    int wait_status;

    struct user_regs_struct temp_registers;
    printf("\r\033[1;35mdragonfly>\033[0m stats (target:\033[1;32m%s\033[0m, pid:\033[1;32m%d\033[0m)\n",
     debugee, child_pid);

    while (1) {
        
        // insert our fuzzcase into the heap
        unsigned char* input_mutated = get_fuzzcase();
        insert_fuzzcase(input_mutated, child_pid);

        resume_execution(child_pid);

        // wait for debuggee to finish/reach breakpoint
        wait(&wait_status);
        if (WIFSTOPPED(wait_status)) {
            // 5 means we hit a bp
            if (WSTOPSIG(wait_status) == 5) {
                temp_registers = get_regs(child_pid, temp_registers);
                if (temp_registers.rip - 1 != end_addr) {
                    add_code_coverage(child_pid, temp_registers);
                }
            }
            else {
                // for this binary, if we're here, that means we crashed
                // will need more robust handling for real targets
                crashes++;
                temp_registers = get_regs(child_pid, temp_registers);
                log_crash(input_mutated, temp_registers);
            }
        }
        else {
            fprintf(stderr, "\033[1;35mdragonfly>\033[0m error (%d) during ", errno);
            perror("wait");
            return;
        }

        // restore writable memory from /proc/$pid/maps to its state at Start
        restore_snapshot(snapshot_buf, child_pid);

        // restore registers to their state at Start
        set_regs(child_pid, snapshot_registers);
        iterations++;
        if (iterations % 50000 == 0) {
            clock_t total = clock() - t;
            double time_taken = ((double)total)/CLOCKS_PER_SEC;
            float million_iterations = (float)iterations / 1000000;
            int result = (int) iterations / time_taken;
            print_stats(result, crashes, million_iterations);
        }
    }
}

void sig_handler(int s) {
    printf("\e[?25h");
    printf("\n\n\n\n\n\n\033[1;35mdragonfly>\033[0m caught user-interrupt, exiting...\n");
    printf("\n");
    exit(0);
}

void scan_input() {
 
    // reading in data from the jpeg that will be our seed data for mutation
    char* filename = "Canon_40D.jpg";
    FILE *fileptr;
    fileptr = fopen(filename, "rb");
    if (fileptr == NULL) {
        fprintf(stderr, "\033[1;35mdragonfly>\033[0m error (%d) during ", errno);
        perror("fopen");
        exit(errno);
    }
 
    fread(input_prototype, 1, prototype_count, fileptr);
    fclose(fileptr);
}

void execute_debugee(char* debugee) {
 
    // request via PTRACE_TRACEME that the parent trace the child
    long ptrace_result = ptrace(PTRACE_TRACEME, 0, 0, 0);
    if (ptrace_result == -1) {
        fprintf(stderr, "\033[1;35mdragonfly>\033[0m error (%d) during ", errno);
        perror("ptrace");
        exit(errno);
    }
 
    // disable ASLR
    int personality_result = personality(ADDR_NO_RANDOMIZE);
    if (personality_result == -1) {
        fprintf(stderr, "\033[1;35mdragonfly>\033[0m error (%d) during ", errno);
        perror("personality");
        exit(errno);
    }
 
    // dup both stdout and stderr and send them to /dev/null
    int fd = open("/dev/null", O_WRONLY);
    dup2(fd, 1);
    dup2(fd, 2);
    close(fd);
 
    // exec our debugee program, NULL terminated to avoid Sentinel compilation
    // warning. this replaces the fork() clone of the parent with the 
    // debugee process 
    int execl_result = execl(debugee, debugee, NULL);
    if (execl_result == -1) {
        fprintf(stderr, "\033[1;35mdragonfly>\033[0m error (%d) during ", errno);
        perror("execl");
        exit(errno);
    }
}

void execute_debugger(pid_t child_pid) {
 
    printf("\033[1;35mdragonfly>\033[0m debuggee pid: %d\n", child_pid);
 
    printf("\033[1;35mdragonfly>\033[0m setting 'start/end' breakpoints:\n\n   "
    "start-> 0x%llx\n   end  -> 0x%llx\n\n", start_addr, end_addr);
 
    int wait_status;
    unsigned instruction_counter = 0;
    struct user_regs_struct registers;
  
    // will SIGTRAP before any subsequent calls to exec
    wait(&wait_status);

    // retrieve the original data at start_addr
    long long unsigned start_val = get_value(child_pid, start_addr);

    // set breakpoint on start_addr
    set_breakpoint(start_addr, start_val, child_pid);

    // retrieve the original data at end_addr
    long long unsigned end_val = get_value(child_pid, end_addr);

    // set breakpoint on end_addr
    set_breakpoint(end_addr, end_val, child_pid);
    //printf("\033[1;35mdragonfly>\033[0m breakpoints set\n");

    //printf("\033[1;35mdragonfly>\033[0m resuming debugee execution...\n");
    resume_execution(child_pid);

    // we've now resumed execution and should stop on our first bp
    wait(&wait_status);
    if (WIFSTOPPED(wait_status)) {
        if (WSTOPSIG(wait_status) == 5) {
            registers = get_regs(child_pid, registers);
            //printf("\033[1;35mdragonfly>\033[0m reached breakpoint at: 0x%llx\n", (registers.rip - 1));
        }
        else {
            printf("dragonfly > Debugee signaled, reason: %s\n",
             strsignal(WSTOPSIG(wait_status)));
        }
    }
    else {
        fprintf(stderr, "\033[1;35mdragonfly>\033[0m error (%d) during ", errno);
        perror("ptrace");
        return;
    }

    // now that we've broken on start, we can revert the breakpoint
    revert_breakpoint(start_addr, start_val, child_pid);

    // rewind rip backwards by one since its at start+1
    // reset registers
    registers.rip -= 1;
    set_regs(child_pid, registers);

    //printf("\033[1;35mdragonfly>\033[0m setting dynamic breakpoints...\n");
    set_dynamic_breakpoints(child_pid);

    // take a snapshot of the writable memory sections in /proc/$pid/maps
    printf("\033[1;35mdragonfly>\033[0m collecting snapshot data\n");
    unsigned char* snapshot_buf = create_snapshot(child_pid);

    // take a snapshot of the registers in this state
    struct user_regs_struct snapshot_registers = get_regs(child_pid, snapshot_registers);

    // snapshot capturing complete, ready to fuzz
    printf("\033[1;35mdragonfly>\033[0m snapshot collection complete\n"
    "\033[1;35mdragonfly>\033[0m press any key to start fuzzing!\n");
    getchar();

    system("clear");
    fuzzer(child_pid, snapshot_buf, snapshot_registers);
}

int main(int argc, char* argv[]) {
 
    // setting up a signal handler so we can exit gracefully from our
    // continuous fuzzing loop that is a while true
    struct sigaction sig_int_handler;
    sig_int_handler.sa_handler = sig_handler;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;
    sigaction(SIGINT, &sig_int_handler, NULL);
 
    srand((unsigned)time(NULL));

    // scan prototypical input into memory so that it can be used
    // to seed fuzz cases
    scan_input();
    
    printf("\n");

    pid_t child_pid = fork();
    if (child_pid == 0) {
        //we're the child process here
        execute_debugee(debugee);
    }
 
    else {
        //we're the parent process here
        execute_debugger(child_pid);
    }
 
    return 0;    
}
