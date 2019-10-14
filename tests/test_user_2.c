#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h> /* uintmax_t */
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>


int main(int argc, char **argv)
{
    int fd;
    long page_size;
    int *address1, *address2;
    uintptr_t paddr;

    if (argc < 2) {
        printf("Usage: %s <mmap_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    page_size = sysconf(_SC_PAGE_SIZE);
    printf("open pathname = %s\n", argv[1]);
    fd = open(argv[1], O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open");
        assert(0);
    }
    printf("fd = %d\n", fd);

    /* mmap twice for double fun. */
    puts("mmap 1");
    address1 = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (address1 == MAP_FAILED) {
        perror("mmap");
        assert(0);
    }
    puts("mmap 2");
    address2 = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (address2 == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }
    assert(address1 != address2);

    address1[0] = 10;
    printf("address2[0] = %d\n", address2[0]);
    close(fd);
    return 0;
}