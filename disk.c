#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#include "disk.h"

#define handle_error(msg) do { perror(msg); exit(EXIT_FAILURE);} while (0)


void get_disk_info(BootEntry * disk) {
    printf("Number of FATs = %d\n"
            "Number of bytes per sector = %d\n"
            "Number of sectors per cluster = %d\n"
            "Number of reserved sectors = %d\n",
            disk->BPB_NumFATs, disk->BPB_BytsPerSec, disk->BPB_SecPerClus, disk->BPB_RsvdSecCnt);
    fflush(stdout);
}

BootEntry * mmap_disk(int argc_i, char* argv[]) {
    struct stat sb;
    size_t size;
    int fd = open(argv[argc_i], O_RDONLY);
    if (fd == -1) {
            handle_error("open");
    }
    fstat (fd, &sb);
    size = sb.st_size;
    BootEntry * disk = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED) {
            handle_error("mmap disk");
    }
    return disk;
}
