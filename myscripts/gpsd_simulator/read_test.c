#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#define SIZE 128
char buffer[SIZE];

int main(int argc, char *argv[]){
    if(argc == 1){
        printf("%s file1 file2\n", argv[0]);
        return -1;
    }

    int flags;

    int fd1 = open(argv[1], O_RDONLY);
    flags = fcntl(fd1, F_GETFL, 0);
    fcntl(fd1, F_SETFL, flags | O_NONBLOCK);

    int fd2 = open(argv[2], O_RDONLY);
    flags = fcntl(fd2, F_GETFL, 0);
    fcntl(fd2, F_SETFL, flags | O_NONBLOCK);

    for(;;){
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd1, &fds);
        FD_SET(fd2, &fds);

        struct timeval t1 = {.tv_usec = 500000};
        struct timeval t2 = {.tv_usec = 500000};

        int rc = select(fd2 + 1, &fds, NULL, NULL, &t1);
        printf("rc = %d; ", rc);
        if(rc < 0) break;

        memset(buffer, 0, SIZE);
        if(FD_ISSET(fd1, &fds)) {
            read(fd1, buffer, SIZE);
            printf("fd1: %s; ", buffer);
        }else{
            printf("fd2: %s; ", "N/A");
        }

        memset(buffer, 0, SIZE);
        if(FD_ISSET(fd2, &fds)) {
            read(fd2, buffer, SIZE);
            printf("fd2: %s\n", buffer);
        }else{
            printf("fd2: %s\n", "N/A");
        }

        //rc = select(0, NULL, NULL, NULL, &t2);
        usleep(500000);
        //printf("rc2 = %d; ", rc);
        //if (rc < 0) break;

    }
    return 0;
}
