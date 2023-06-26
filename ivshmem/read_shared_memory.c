#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define NAME "/my_shmem0"
#define SIZE 512

int main() {
    int i, fd;
    char *data;

    fd = shm_open(NAME, O_RDONLY, 0600);
    if (fd < 0) {
        perror ("shm_open()");
        return EXIT_FAILURE;
    }

    ftruncate(fd, SIZE);
    data = (char *) mmap(0, SIZE, PROT_READ, MAP_SHARED, fd, 0);
    printf("receiver mapped address: %p\n", data);

    printf("%s\n", data);

    munmap(data, SIZE);
    close(fd);

    return EXIT_SUCCESS;
}
