#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h> 
#include <fcntl.h>

int crashes = 0;

struct ORIGINAL_FILE {
    char * data;
    size_t length;
};

struct ORIGINAL_FILE get_data(char* fuzz_target) {

    FILE *fileptr;
    char *clone_data;
    long filelen;

    // open file in binary read mode
    // jump to end of file, get length
    // reset pointer to beginning of file
    fileptr = fopen(fuzz_target, "rb");
    if (fileptr == NULL) {
        printf("[!] Unable to open fuzz target, exiting...\n");
        exit(1);
    }
    fseek(fileptr, 0, SEEK_END);
    filelen = ftell(fileptr);
    rewind(fileptr);

    // cast malloc as char ptr
    // ptr offset * sizeof char = data in .jpeg
    clone_data = (char *)malloc(filelen * sizeof(char));

    // get length for struct returned
    size_t length = filelen * sizeof(char);

    // read in the data
    fread(clone_data, filelen, 1, fileptr);
    fclose(fileptr);

    struct ORIGINAL_FILE original_file;
    original_file.data = clone_data;
    original_file.length = length;

    return original_file;
}

void create_new(struct ORIGINAL_FILE original_file, size_t mutations) {

    //
    //----------------MUTATE THE BITS-------------------------
    //
    int* picked_indexes = (int*)malloc(sizeof(int)*mutations);
    for (int i = 0; i < (int)mutations; i++) {
        picked_indexes[i] = rand() % original_file.length;
    }

    char * mutated_data = (char*)malloc(original_file.length);
    memcpy(mutated_data, original_file.data, original_file.length);

    for (int i = 0; i < (int)mutations; i++) {
        char current = mutated_data[picked_indexes[i]];
        
        // get value of char at this index of the orignal data
        // and convert it into a decimal between 0-255
        int decimal = ((int)current & 0xff);

        // figure out what bit to flip in this 'decimal' byte
        int bit_to_flip = rand() % 8;
        
        //flip that bit
        decimal ^= 1 << bit_to_flip;
        
        mutated_data[picked_indexes[i]] = (char)decimal;
    }

    //
    //---------WRITING THE MUTATED BITS TO NEW FILE-----------
    //
    FILE *fileptr;
    fileptr = fopen("mutated.jpeg", "wb");
    if (fileptr == NULL) {
        printf("[!] Unable to open mutated.jpeg, exiting...\n");
        exit(1);
    }
    // buffer to be written from,
    // size in bytes of elements,
    // how many elements,
    // where to stream the output to :)
    fwrite(mutated_data, 1, original_file.length, fileptr);
    fclose(fileptr);
    free(mutated_data);
    free(picked_indexes);
}

void exif(int iteration) {
    
    //fileptr = popen("exiv2 pr -v mutated.jpeg >/dev/null 2>&1", "r");
    char* file = "exiv2";
    char* argv[4];
    argv[0] = "pr";
    argv[1] = "-v";
    argv[2] = "mutated.jpeg";
    argv[3] = NULL;
    pid_t child_pid;
    int child_status;

    child_pid = fork();
    if (child_pid == 0) {
        // this means we're the child process
        int fd = open("/dev/null", O_WRONLY);

        // dup both stdout and stderr and send them to /dev/null
        dup2(fd, 1);
        dup2(fd, 2);
        close(fd);

        execvp(file, argv);
        // shouldn't return, if it does, we have an error with the command
        printf("[!] Unknown command for execvp, exiting...\n");
        exit(1);
    }
    else {
        // this is run by the parent process
        do {
            pid_t tpid = waitpid(child_pid, &child_status, WUNTRACED |
             WCONTINUED);
            if (tpid == -1) {
                printf("[!] Waitpid failed!\n");
                perror("waitpid");
            }
            if (WIFEXITED(child_status)) {
                //printf("WIFEXITED: Exit Status: %d\n", WEXITSTATUS(child_status));
            } else if (WIFSIGNALED(child_status)) {
                crashes++;
                int exit_status = WTERMSIG(child_status);
                printf("\r[>] Crashes: %d", crashes);
                fflush(stdout);
                char command[50];
                sprintf(command, "cp mutated.jpeg ccrashes/%d.%d", iteration, 
                exit_status);
                system(command);
            } else if (WIFSTOPPED(child_status)) {
                printf("WIFSTOPPED: Exit Status: %d\n", WSTOPSIG(child_status));
            } else if (WIFCONTINUED(child_status)) {
                printf("WIFCONTINUED: Exit Status: Continued.\n");
            }
        } while (!WIFEXITED(child_status) && !WIFSIGNALED(child_status));
    }
}

int main(int argc, char** argv) {

    if (argc < 3) {
        printf("Usage: ./cfuzz <valid jpeg> <num of fuzz iterations>\n");
        printf("Usage: ./cfuzz Canon_40D.jpg 10000\n");
        exit(1);
    }

    // get our random seed
    srand((unsigned)time(NULL));

    char* fuzz_target = argv[1];
    struct ORIGINAL_FILE original_file = get_data(fuzz_target);
    printf("[>] Size of file: %ld bytes.\n", original_file.length);
    size_t mutations = (original_file.length - 4) * .01;
    printf("[>] Flipping %ld bits.\n", mutations);

    int iterations = atoi(argv[2]);
    printf("[>] Fuzzing for %d iterations...\n", iterations);
    for (int i = 0; i < iterations; i++) {
        create_new(original_file, mutations);
        exif(i);
    }
    
    return 0;
}
