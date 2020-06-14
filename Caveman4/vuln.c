#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>

struct ORIGINAL_FILE {
    char * data;
    size_t length;
};

struct ORIGINAL_FILE get_bytes(char* fileName) {

    FILE *filePtr;
    char* buffer;
    long fileLen;

    filePtr = fopen(fileName, "rb");
    if (!filePtr) {
        printf("[>] Unable to open %s\n", fileName);
        exit(-1);
    }
    
    if (fseek(filePtr, 0, SEEK_END)) {
        printf("[>] fseek() failed, wtf?\n");
        exit(-1);
    }

    fileLen = ftell(filePtr);
    if (fileLen == -1) {
        printf("[>] ftell() failed, wtf?\n");
        exit(-1);
    }

    errno = 0;
    rewind(filePtr);
    if (errno) {
        printf("[>] rewind() failed, wtf?\n");
        exit(-1);
    }

    long trueSize = fileLen * sizeof(char);
    printf("[>] %s is %ld bytes.\n", fileName, trueSize);
    buffer = (char *)malloc(fileLen * sizeof(char));
    fread(buffer, fileLen, 1, filePtr);
    fclose(filePtr);

    struct ORIGINAL_FILE original_file;
    original_file.data = buffer;
    original_file.length = trueSize;

    return original_file;
}

void check_one(char* buffer, int check) {

    if (buffer[check] == '\x6c') {
        return;
    }
    else {
        printf("[>] Check 1 failed.\n");
        exit(-1);
    }
}

void check_two(char* buffer, int check) {

    if (buffer[check] == '\x57') {
        return;
    }
    else {
        printf("[>] Check 2 failed.\n");
        exit(-1);
    }
}

void check_three(char* buffer, int check) {

    if (buffer[check] == '\x21') {
        return;
    }
    else {
        printf("[>] Check 3 failed.\n");
        exit(-1);
    }
}

void vuln(char* buffer, size_t length) {

    printf("[>] Passed all checks!\n");
    char vulnBuff[20];

    memcpy(vulnBuff, buffer, length);

}

int main(int argc, char *argv[]) {
    
    /*
    if (argc < 2 || argc > 2) {
        printf("[>] Usage: vuln example.txt\n");
        exit(-1);
    }

    char *filename = argv[1];
    */

    char *filename = "Canon_40D.jpg";
    printf("[>] Analyzing file: %s.\n", filename);

    struct ORIGINAL_FILE original_file = get_bytes(filename);

    int checkNum1 = (int)(original_file.length * .33);
    printf("[>] Check 1 no.: %d\n", checkNum1);

    int checkNum2 = (int)(original_file.length * .5);
    printf("[>] Check 2 no.: %d\n", checkNum2);

    int checkNum3 = (int)(original_file.length * .67);
    printf("[>] Check 3 no.: %d\n", checkNum3);

    check_one(original_file.data, checkNum1);
    check_two(original_file.data, checkNum2);
    check_three(original_file.data, checkNum3);
    
    vuln(original_file.data, original_file.length);
    

    return 0;
}
