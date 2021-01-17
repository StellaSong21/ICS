#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    int fd;
    unsigned long long i = 0;
    unsigned long long j = 0;
    char* crash_me = NULL;

    if (argc != 2) {
        return 1;
    }

    fd = open(argv[1], O_RDONLY);

    if (fd == -1) {
        return 1;
    }

    read(fd, &i, sizeof(i));
    read(fd, &j, sizeof(j));

    if (i == 0xdeadbeef && j == 0xcafebabe) {
        *crash_me = 'A';
    }

    return 0;
}