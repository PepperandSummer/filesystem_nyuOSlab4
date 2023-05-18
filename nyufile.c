#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "disk.h"
#include "directory.h"
#include "recover.h"

void usage() {
    printf("Usage: ./nyufile disk <options>\n");
    printf("  -i                     Print the file system information.\n");
    printf("  -l                     List the root directory.\n");
    printf("  -r filename [-s sha1]  Recover a contiguous file.\n");
    printf("  -R filename -s sha1    Recover a possibly non-contiguous file.\n");
    exit(1);
}
int main(int argc, char* argv[]) {
    int ch;
    int i_flag = 0; //option -i
    int l_flag = 0;
    int r_flag = 0;
    int R_flag = 0;
    int R_sha1 = 0; // option -R -s
    int flag = 0; // invalid parameter

    char * r_optarg;
    char * R_optarg;
    char * SHA_1;
    while((ch = getopt(argc,argv,"ilr:R:s:")) != -1)
    {   

        flag = 1;
        switch(ch)
        {
        case 'i':
            i_flag = 1;
            //printf("option %c\n", ch);
            break;
        case 'l':
            l_flag = 1;
            //printf("option %c\n", ch);
            break;
        case 'r':
            r_flag = 1;
            r_optarg = optarg;
            //printf("option %c, filename = %s\n", ch, optarg);
            break;
        case 'R':
            R_flag = 1;
            R_optarg = optarg;
            //printf("argc : %d optind: %d\n", argc, optind);
            //printf("option %c, filename = %s\n", ch, optarg);
            break;
        case 's':
            R_sha1 = 1;
            SHA_1 = optarg;
            //printf("option %c, filename = %s\n", ch, optarg);
            break;
        default:
            //printf("this is default.\n");
            usage();
            break;
            //printf("other option:%c\n",ch);
        }
    }

    if ((ch == 'R'&& R_sha1 != 1) || flag == 0 || optind == argc) { //case: ./nyufile xxx 0 or case: ./nyufile
        usage();
    }
    //printf("optind %d\n", optind);
    int fd = get_fd(optind, argv);
    BootEntry * disk = mmap_disk(optind, argv); //open disk file
    if(i_flag == 1) {
        get_disk_info(disk);
        fflush(stdout);
    }
    else if(l_flag == 1) {
        BootEntry * disk = mmap_disk(optind, argv);
        get_rootdir_entry(fd, disk);
        fflush(stdout);
    }
    else if(R_sha1 == 1) {
        if(r_flag == 1) {
            if(r_optarg == NULL || SHA_1 == NULL){
                usage();
            } 
            int r_fd = r_get_fd(optind, argv);
            BootEntry * r_disk = r_get_disk(optind, argv);
            recover_file(r_fd, r_disk, r_optarg, SHA_1);
            fflush(stdout);
        }
        else if(R_flag == 1 ) {
            if(R_optarg == NULL || SHA_1 == NULL){
                usage();
            }
            int r_fd = r_get_fd(optind, argv);
            BootEntry * r_disk = r_get_disk(optind, argv);
            recover_file(r_fd, r_disk, R_optarg, SHA_1);
        }
        else{
            usage();
        }
    }
    else if(r_flag == 1 && R_flag == 0){
        if(r_optarg == NULL){
            usage();
        } 
        int r_fd = r_get_fd(optind, argv);
        BootEntry * r_disk = r_get_disk(optind, argv);
        recover_file(r_fd, r_disk, r_optarg, NULL);
        fflush(stdout);
    } else {
        usage();
    }
    
}

/**
 * Usage: ./nyufile disk <options>
  -i                     Print the file system information.
  -l                     List the root directory.
  -r filename [-s sha1]  Recover a contiguous file.
  -R filename -s sha1    Recover a possibly non-contiguous file.
*/