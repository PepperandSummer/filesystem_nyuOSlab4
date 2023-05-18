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

int get_fd(int argc_i, char* argv[]) {
    int fd = open(argv[argc_i], O_RDONLY);
    if (fd == -1) {
            handle_error("open");
    }
    return fd;
}

unsigned char * get_disk_content(int fd) {
    struct stat sb;
    size_t size;
    fstat (fd, &sb);
    size = sb.st_size;
    unsigned char * disk_content = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if (disk_content == NULL) {
            handle_error("disk directory mmap");
    }
    return disk_content;
}

/**
 * | reserved area(including Boot sector) | FAT 1 | FAT 2 | Data Area |
 * 
 * Size in sectors of the reserved area: unsigned short BPB_RsvdSecCnt;    
*/
unsigned int get_first_data_sector(BootEntry * disk) {
    unsigned int first_data_sector =  disk->BPB_RsvdSecCnt + (disk->BPB_FATSz32 * disk->BPB_NumFATs);
    return first_data_sector;
}

//find the the first sector of cluster (if root cluster is 2)
//convert to bytes
DirEntry * get_dir_entry(unsigned char * disk_content, BootEntry * disk, unsigned int cluster) {

    unsigned int first_data_sector = get_first_data_sector(disk);
    unsigned int dir_first_sector = ((cluster - 2) * disk->BPB_SecPerClus) + first_data_sector;
    unsigned int dir_start_bytes = dir_first_sector * disk->BPB_BytsPerSec;

    DirEntry * dir_entry = (DirEntry *)(disk_content + dir_start_bytes);
    if (dir_entry == NULL){
        handle_error("rootdir mmap");
    }
    return dir_entry;
}



//have to identify a item is a file or a directory...

void get_rootdir_entry(int fd, BootEntry * disk) {
    unsigned char * disk_content = get_disk_content(fd);
    unsigned int total_entries = 0; // total_entries
    unsigned int cluster_NO = disk->BPB_RootClus; // root cluster = 2
    //unsigned int * fat_start = (unsigned int *)(disk_content + disk->BPB_RsvdSecCnt * disk->BPB_BytsPerSec); // fat1 location
    unsigned int cluster_size = disk->BPB_SecPerClus * disk->BPB_BytsPerSec; // one cluster size (bytes)
    //unsigned int cluster_index_size = 4; 
    DirEntry * dir_entry = NULL; //initialize
    do{
        dir_entry = get_dir_entry(disk_content, disk, cluster_NO);
        // cluster_size / sizeof(DirEntry) = how many dir_entry
        // HELLO.TXT--------dir_entry
        // DIR/-------------dir_entry
        // EMPTY------------dir_entry
        for(unsigned int i = 0; i < cluster_size / sizeof(DirEntry); i++) {
            if (dir_entry->DIR_Name[0] == 0x00) {
                break;
            }
            /**
             * HELLO.TXT (size = 14, starting cluster = 3)
            */
            if (dir_entry->DIR_Name[0] != 0xE5) {
                if(dir_entry->DIR_Attr != 0x10) {
                    // filename
                    for (int n_i = 0; n_i < 8; n_i++) {
                        if(dir_entry->DIR_Name[n_i] != ' ') {
                            printf("%c", dir_entry->DIR_Name[n_i]);
                        } else {
                            break;
                        }
                    }
                    //. file type
                    if(dir_entry->DIR_Name[8] != ' '){
                        printf(".");
                    }
                    for (int n_i = 8; n_i < 12; n_i++) {
                        if(dir_entry->DIR_Name[n_i] != ' ') {
                            printf("%c", dir_entry->DIR_Name[n_i]);
                        } else {
                            break;
                        }
                    }
                    
                    printf(" (size = %u", dir_entry->DIR_FileSize);
                    if(dir_entry->DIR_FileSize > 0) {
                        printf(", starting cluster = %u)\n", dir_entry->DIR_FstClusLO);
                    } else {
                        printf(")\n");
                    }   
                } else {// if this is a directory
                    int n_i = 0;
                    while(dir_entry->DIR_Name[n_i] != ' ') {
                        printf("%c",dir_entry->DIR_Name[n_i]);
                        n_i++;
                    }
                    printf("/ ");
                    printf("(starting cluster = %u)\n", (dir_entry->DIR_FstClusHI << 16 | dir_entry->DIR_FstClusLO));
                }
                total_entries++;
            }
            dir_entry++;
        }
        unsigned int *fat_next_cluster_NO = (unsigned int *)(disk_content + disk->BPB_RsvdSecCnt * disk->BPB_BytsPerSec + 4 * cluster_NO);
        //printf("fat_next_cluster_NO is %ls", fat_next_cluster_NO);
        if (* fat_next_cluster_NO >= 0x0ffffff8 || * fat_next_cluster_NO == 0x00) {
            break;
        }
        cluster_NO = * fat_next_cluster_NO;
    }while(1);
    printf("Total number of entries = %u\n", total_entries);
    // get the root dir_entry

}