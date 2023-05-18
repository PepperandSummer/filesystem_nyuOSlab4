#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <openssl/sha.h>


#include "disk.h"
#include "directory.h"
#include "recover.h"
#define SHA_DIGEST_LENGTH 20
#define MAX_CLUSTER_COUNT 20
#define handle_error(msg) do { perror(msg); exit(EXIT_FAILURE);} while (0)

static unsigned int cluster_chain[MAX_CLUSTER_COUNT];

int r_get_fd(int argc_i, char* argv[]) {
    int fd = open(argv[argc_i], O_RDWR);
    if (fd == -1) {
            handle_error("open");
    }
    return fd;
}

unsigned char * r_get_disk_content(int fd) {
    struct stat sb;
    size_t size;
    fstat (fd, &sb);
    size = sb.st_size;
    unsigned char * disk_content = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk_content == NULL) {
            handle_error("disk directory mmap");
    }
    return disk_content;
}

BootEntry * r_get_disk(int argc_i, char* argv[]) {
    struct stat sb;
    size_t size;
    int fd = open(argv[argc_i], O_RDWR);
    if (fd == -1) {
            handle_error("open");
    }
    fstat (fd, &sb);
    size = sb.st_size;
    BootEntry * disk = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (disk == MAP_FAILED) {
            handle_error("mmap disk");
    }
    return disk;
}

unsigned char * get_filename(DirEntry * dir_entry) {
    unsigned char * filename = malloc((11 + 1) * sizeof(unsigned char *)); // storage space filename
    unsigned int index = 0;
    for (int i = 0; i < 8; i++) {
        if(dir_entry->DIR_Name[i] != ' ') {
            //printf("%c", dir_entry->DIR_Name[i]);
            filename[index] = dir_entry->DIR_Name[i];
            index++;
        } else {
            break;
        }
    }
    //. file type
    if(dir_entry->DIR_Name[8] != ' '){
        //printf(".");
        filename[index] = '.';
        index ++;
    }

    for (int i = 8; i < 11; i++) {
        if(dir_entry->DIR_Name[i] != ' ') {
            //printf("%c", dir_entry->DIR_Name[i]);
            filename[index] = dir_entry->DIR_Name[i];
            index++;
        } else {
            break;
        }
    }
    filename[index] = '\0';
    return filename;

}

unsigned int count_file_cluster(DirEntry * dir_entry, unsigned int cluster_size) {
    int cluster_count = 0;
    if (dir_entry->DIR_FileSize == 0) {
        cluster_count = 1;
    } else {
        cluster_count = (dir_entry->DIR_FileSize + cluster_size - 1) / cluster_size;
    }
    return cluster_count;
}

int update_FATs(BootEntry * disk, unsigned char * disk_content, unsigned int n_cluster, unsigned int start_cluster) {
    unsigned int FAT_bytes = disk->BPB_BytsPerSec * disk->BPB_FATSz32;
    //unsigned int cluster_size = disk->BPB_SecPerClus * disk->BPB_BytsPerSec;
    //unsigned fats_per_cluster = cluster_size / 4;
    //unsigned int * FAT_start = (unsigned int *)(disk_content + disk->BPB_RsvdSecCnt * disk->BPB_BytsPerSec);
    for(unsigned int i = 0; i < n_cluster - 1; i ++) {
        // inside loop update all fat table
        // unsigned int offset = 1;
        for(int j = 0; j < disk->BPB_NumFATs; j++) {
            unsigned int * fat = (unsigned int *)((disk_content + disk->BPB_RsvdSecCnt * disk->BPB_BytsPerSec) + 4 * start_cluster + j * FAT_bytes);
            * fat = start_cluster + 1;
            //* fat = cluster_chain[i + 1];
            //printf("%u\n",fat);
        }
        start_cluster = start_cluster + 1;
        // start_cluster = cluster_chain[i + 1];
    }
    for(int k = 0; k < disk->BPB_NumFATs; k++) {
            unsigned int *fat = (unsigned int *)((disk_content + disk->BPB_RsvdSecCnt * disk->BPB_BytsPerSec) + 4 * start_cluster + k * FAT_bytes);
            * fat = 0x0ffffff8;
    }
    return 0;
}

int check_SHA(DirEntry * dir_entry, char * input_sha, unsigned char * file_data) {
    unsigned char * file_sha = generate_SHA(dir_entry, file_data);
    char p_file_sha[SHA_DIGEST_LENGTH * 2 + 1];
    // file_sha: { 0x12, 0xAB, 0x34, 0xCD }
    // p_file_sha: "12ab34cd"
    for(int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        sprintf((p_file_sha + (i * 2)), "%02x", file_sha[i]);
    }
    p_file_sha[SHA_DIGEST_LENGTH * 2] = '\0';
    //free(file_sha);
    if(strcmp(input_sha, p_file_sha)== 0) {
        return 0;
    } else {
        return -1;
    }
}

unsigned char * generate_SHA(DirEntry * dir_entry,unsigned char * file_data){
    unsigned char *file_sha = (unsigned char*)malloc(SHA_DIGEST_LENGTH * sizeof(unsigned char*));
    SHA1(file_data, dir_entry->DIR_FileSize, file_sha);
    return file_sha;
}


unsigned char * get_file_content(int fd, BootEntry * disk, DirEntry * dir_entry) {
    unsigned char * disk_content = r_get_disk_content(fd);
    unsigned int cluster_size = disk->BPB_SecPerClus * disk->BPB_BytsPerSec;
    //unsigned int * fat_start = (unsigned int *)(disk_content + disk->BPB_RsvdSecCnt * disk->BPB_BytsPerSec); // fat1 location
    unsigned int first_data_sector = get_first_data_sector(disk);
   
    unsigned int file_size = dir_entry->DIR_FileSize;
    unsigned int n_clusters = count_file_cluster(dir_entry, cluster_size);
    //unsigned int * cluster_chain = (unsigned int *)malloc(sizeof(unsigned int) * n_clusters);
    unsigned int file_start_cluster = 0;
    unsigned char * file_data = (unsigned char*)malloc((file_size + 1) * sizeof(unsigned char *));
    unsigned int data_index = 0;
    unsigned int file_data_sector = 0;
    unsigned int sector_start_bytes = 0;
    if (n_clusters > 1) {
        file_start_cluster = dir_entry->DIR_FstClusHI << 16 | dir_entry->DIR_FstClusLO;
        
        
        for (unsigned int i = 0; i < n_clusters; i++) {
            //current cluster size
            //next cluster
            //unsigned int offset = 1;
            file_data_sector = ((file_start_cluster - 2) * disk->BPB_SecPerClus) + first_data_sector;
            sector_start_bytes = file_data_sector * disk->BPB_BytsPerSec;
            unsigned int last_size = cluster_size; 
            if(i == n_clusters - 1) {
                last_size = file_size - i * cluster_size;
            }
            for(unsigned int j = 0; j < last_size; j++) {
                file_data[data_index] = disk_content[sector_start_bytes + j];
                data_index++;
            }
            //last_size = 0;
            cluster_chain[i] = file_start_cluster;
            file_start_cluster++;
            // if (get_next_cluster(disk_content, disk, file_start_cluster) != 0){
            //     file_start_cluster = get_next_cluster(disk_content, disk, file_start_cluster);
            // } else {
            //     break;
            // }

        }

    } else {
        file_start_cluster = dir_entry->DIR_FstClusLO;
        unsigned int file_data_sector = ((file_start_cluster - 2) * disk->BPB_SecPerClus) + first_data_sector;
        unsigned int sector_start_bytes = file_data_sector * disk->BPB_BytsPerSec;
        for(unsigned int i = 0; i < file_size; i++) {
            file_data[data_index] = disk_content[sector_start_bytes + i];
            data_index++;
        }
        //printf("file_start_cluster %u---\n",file_start_cluster);
    }
     
    return file_data;

}

int get_deleted_dir_entry(int fd, BootEntry * disk, char * optarg, char * SHA_1) {
    unsigned char * disk_content = r_get_disk_content(fd); // get disk content
    //unsigned int total_entries = 0; // total_entries
    unsigned int cluster_NO = disk->BPB_RootClus; // get root cluster
    //unsigned int * fat_start = (unsigned int *)(disk_content + disk->BPB_RsvdSecCnt * disk->BPB_BytsPerSec); // fat1 location
    unsigned int cluster_size = disk->BPB_SecPerClus * disk->BPB_BytsPerSec; // one cluster size (bytes)
    DirEntry * dir_entry = NULL; //initialize
    int file_count = 0; // flag
    //unsigned int tmp_cluster_NO = 0;
    DirEntry * tmp_dir_entry = NULL;
    unsigned char * file_data = NULL;

    // find the deleted file from root directory
    do{
        dir_entry = get_dir_entry(disk_content, disk, cluster_NO); // get dir_entry

        // iterate every dir_entry in the current cluster
        for(unsigned int i = 0; i < cluster_size / sizeof(DirEntry); i++) {
            if (dir_entry->DIR_Name[0] == 0x00) {
                break;
            }
            // first it's not a directory but a deleted file
            // dir_entry->DIR_Attr != 0x10 && 
            if(dir_entry->DIR_Attr != 0x10 && dir_entry->DIR_Name[0] == 0xE5) {
                //printf("file size is %u\n", dir_entry->DIR_FileSize);
                // compare the 2 nd charater between optarg and DIR_name
                
                //if (optarg[1] == dir_entry->DIR_Name[1]) {
                    unsigned char * p_filename = get_filename(dir_entry); // get dir_filename
                    //printf("p_filename is %s", p_filename);
                    if(strcmp((optarg + 1), (char *)(p_filename + 1)) == 0) {
                        if(SHA_1) {
                            //check SHA_1 with true SHA_1
                            file_data = get_file_content(fd, disk, dir_entry);
                            if(check_SHA(dir_entry, SHA_1, file_data) == 0){
                                file_count = 1;
                                tmp_dir_entry = dir_entry;
                                //tmp_cluster_NO = cluster_NO;
                            }
                        } else {
                            if(file_count < 1) {
                                file_count++;
                                tmp_dir_entry = dir_entry;
                                //tmp_cluster_NO = cluster_NO;
                            } else {
                                printf("%s: multiple candidates found\n",optarg);
                                fflush(stdout);
                                return 0;
                            }
                        }
                        //find a matched file
                        //printf("file is matched.\n");
                    }
                    //free(p_filename);
                //}
            }
            dir_entry++;
        }
        //printf("loop ending...\n");
        unsigned int *fat_next_cluster_NO = (unsigned int *)(disk_content + disk->BPB_RsvdSecCnt * disk->BPB_BytsPerSec + 4 * cluster_NO);
        if (* fat_next_cluster_NO >= 0x0ffffff8 || * fat_next_cluster_NO == 0x00) {
            break;
        }
        cluster_NO = * fat_next_cluster_NO;
    }while(1);
    
    // printf("loop ending...\n");
    unsigned int n_cluster = 0; // how many cluster a file need
    unsigned int file_start_cluster = 0;
    if(file_count == 1) {
        //printf("file_count == 1\n");
        //update root directory
        tmp_dir_entry->DIR_Name[0] = optarg[0];
        // get n_clusters
        n_cluster = count_file_cluster(tmp_dir_entry, cluster_size);
        if (n_cluster > 1) {
            file_start_cluster = tmp_dir_entry->DIR_FstClusHI << 16 | tmp_dir_entry->DIR_FstClusLO;
        } else {
            file_start_cluster = tmp_dir_entry->DIR_FstClusLO;
            //printf("file_start_cluster %u---\n",file_start_cluster);
        }
        int res = update_FATs(disk, disk_content, n_cluster, file_start_cluster);
        if (res != 0) {
            handle_error("update FATs");
        }
        if (SHA_1)
            printf("%s: successfully recovered with SHA-1\n", optarg);
        else{
            printf("%s: successfully recovered\n", optarg);
            
        }
        fflush(stdout);
        return 0;
    }
    return -1;
}

void recover_file(int fd, BootEntry * disk, char * optarg, char * SHA_1) {
    if (get_deleted_dir_entry(fd, disk, optarg, SHA_1) == -1){
        printf("%s: file not found\n", optarg);
    }
}



//reference
// https://stackoverflow.com/questions/53212532/undefined-reference-to-sha1-in-c