#ifndef RECOVER_H
#define RECOVER_H

#include "disk.h"
#include "directory.h"
int r_get_fd(int argc_i, char* argv[]);
unsigned char * r_get_disk_content(int fd);
BootEntry * r_get_disk(int argc_i, char* argv[]);
unsigned char * get_filename(DirEntry * dir_entry);
unsigned int count_file_cluster(DirEntry * dir_entry, unsigned int cluster_size);
int check_SHA(DirEntry * dir_entry, char * input_sha, unsigned char * file_data);
unsigned char * generate_SHA(DirEntry * dir_entry,unsigned char * file_data);
unsigned char * get_file_content(int fd, BootEntry * disk, DirEntry * dir_entry);
int update_FATs(BootEntry * disk, unsigned char * disk_content, unsigned int n_cluster, unsigned int start_cluster);
int get_deleted_dir_entry(int fd, BootEntry * disk, char * optarg, char* SHA_1);
void recover_file(int fd, BootEntry * disk, char * optarg, char* SHA_1);
#endif