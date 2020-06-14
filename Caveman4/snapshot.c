#define _GNU_SOURCE
#include "snapshot.h"
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

unsigned char* create_snapshot(pid_t child_pid) {
 
    struct SNAPSHOT_MEMORY read_memory = {
        {
            // maps_offset
            0x555555756000,
            0x7ffff7dcf000,
            0x7ffff7dd1000,
            0x7ffff7fe0000,
            0x7ffff7ffd000,
            0x7ffff7ffe000,
            0x7ffffffde000
        },
        {
            // snapshot_buf_offset
            0x0,
            0xFFF,
            0x2FFF,
            0x6FFF,
            0x8FFF,
            0x9FFF,
            0xAFFF
        },
        {
            // rdwr length
            0x1000,
            0x2000,
            0x4000,
            0x2000,
            0x1000,
            0x1000,
            0x21000
        }
    };  
 
    unsigned char* snapshot_buf = (unsigned char*)malloc(0x2C000);
 
    // this is just /proc/$pid/mem
    char proc_mem[0x20] = { 0 };
    sprintf(proc_mem, "/proc/%d/mem", child_pid);
 
    // open /proc/$pid/mem for reading
    // hardcoded offsets are from typical /proc/$pid/maps at main()
    int mem_fd = open(proc_mem, O_RDONLY);
    if (mem_fd == -1) {
        fprintf(stderr, "dragonfly> Error (%d) during ", errno);
        perror("open");
        exit(errno);
    }
 
    // this loop will:
    //  -- go to an offset within /proc/$pid/mem via lseek()
    //  -- read x-pages of memory from that offset into the snapshot buffer
    //  -- adjust the snapshot buffer offset so nothing is overwritten in it
    int lseek_result, bytes_read;
    for (int i = 0; i < 7; i++) {
        //printf("dragonfly> Reading from offset: %d\n", i+1);
        lseek_result = lseek(mem_fd, read_memory.maps_offset[i], SEEK_SET);
        if (lseek_result == -1) {
            fprintf(stderr, "dragonfly> Error (%d) during ", errno);
            perror("lseek");
            exit(errno);
        }
 
        bytes_read = read(mem_fd,
            (unsigned char*)(snapshot_buf + read_memory.snapshot_buf_offset[i]),
            read_memory.rdwr_length[i]);
        if (bytes_read == -1) {
            fprintf(stderr, "dragonfly> Error (%d) during ", errno);
            perror("read");
            exit(errno);
        }
    }
 
    close(mem_fd);
    return snapshot_buf;
}

void restore_snapshot(unsigned char* snapshot_buf, pid_t child_pid) {
 
    ssize_t bytes_written = 0;
    // we're writing *from* 7 different offsets within snapshot_buf
    struct iovec local[7];
    // we're writing *to* 7 separate sections of writable memory here
    struct iovec remote[7];
 
    // this struct is the local buffer we want to write from into the 
    // struct that is 'remote' (ie, the child process where we'll overwrite
    // all of the non-heap writable memory sections that we parsed from 
    // proc/$pid/memory)
    local[0].iov_base = snapshot_buf;
    local[0].iov_len = 0x1000;
    local[1].iov_base = (unsigned char*)(snapshot_buf + 0xFFF);
    local[1].iov_len = 0x2000;
    local[2].iov_base = (unsigned char*)(snapshot_buf + 0x2FFF);
    local[2].iov_len = 0x4000;
    local[3].iov_base = (unsigned char*)(snapshot_buf + 0x6FFF);
    local[3].iov_len = 0x2000;
    local[4].iov_base = (unsigned char*)(snapshot_buf + 0x8FFF);
    local[4].iov_len = 0x1000;
    local[5].iov_base = (unsigned char*)(snapshot_buf + 0x9FFF);
    local[5].iov_len = 0x1000;
    local[6].iov_base = (unsigned char*)(snapshot_buf + 0xAFFF);
    local[6].iov_len = 0x21000;
 
    // just hardcoding the base addresses that are writable memory
    // that we gleaned from /proc/pid/maps and their lengths
    remote[0].iov_base = (void*)0x555555756000;
    remote[0].iov_len = 0x1000;
    remote[1].iov_base = (void*)0x7ffff7dcf000;
    remote[1].iov_len = 0x2000;
    remote[2].iov_base = (void*)0x7ffff7dd1000;
    remote[2].iov_len = 0x4000;
    remote[3].iov_base = (void*)0x7ffff7fe0000;
    remote[3].iov_len = 0x2000;
    remote[4].iov_base = (void*)0x7ffff7ffd000;
    remote[4].iov_len = 0x1000;
    remote[5].iov_base = (void*)0x7ffff7ffe000;
    remote[5].iov_len = 0x1000;
    remote[6].iov_base = (void*)0x7ffffffde000;
    remote[6].iov_len = 0x21000;
 
    bytes_written = process_vm_writev(child_pid, local, 7, remote, 7, 0);
    //printf("dragonfly> %ld bytes written\n", bytes_written);
}
