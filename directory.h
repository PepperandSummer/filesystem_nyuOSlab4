#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <stdlib.h>

#include "disk.h"

int get_fd(int argc_i, char* argv[]);

unsigned char * get_disk_content(int fd); //获得磁盘内存 无需创建结构体

unsigned int get_first_data_sector(BootEntry * disk); //location of first data sector

DirEntry * get_dir_entry(unsigned char * disk_content, BootEntry * disk, unsigned int cluster);

void get_rootdir_entry(int fd, BootEntry * disk); // Cluster where the root directory can be found


#endif