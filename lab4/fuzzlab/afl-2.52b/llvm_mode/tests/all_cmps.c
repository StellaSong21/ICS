#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    int sop0, sop1;
    unsigned int usop0, usop1;
    int res = 0;

    usop0 = usop1 = 0;
    if (usop0 == usop1) {
        printf("[+] Unsigned %u == %u\n", usop0, usop1);
    } else {
        printf("[-] Unsigned %u != %u\n", usop0, usop1);
        res = 1;
    }

    usop0 = usop1 = -1;
    if (usop0 == usop1) {
        printf("[+] Unsigned %u == %u\n", usop0, usop1);
    } else {
        printf("[-] Unsigned %u != %u\n", usop0, usop1);
        res = 1;
    }

    usop0 = 0xdeadbeef;
    usop1 = 0xdeadbeee;
    if (usop0 > usop1) {
        printf("[+] Unsigned %u > %u\n", usop0, usop1);
    } else {
        printf("[-] Unsigned %u !> %u\n", usop0, usop1);
        res = 1;
    }

    usop0 = 0xdeadbeee;
    usop1 = 0xdeadbeef;
    if (usop0 < usop1) {
        printf("[+] Unsigned %u < %u\n", usop0, usop1);
    } else {
        printf("[-] Unsigned %u !< %u\n", usop0, usop1);
        res = 1;
    }

    usop0 = 0xdeadbeee;
    usop1 = 0xdeadbeef;
    if (usop0 <= usop1) {
        printf("[+] Unsigned %u <= %u\n", usop0, usop1);
    } else {
        printf("[-] Unsigned %u !<= %u\n", usop0, usop1);
        res = 1;
    }

    usop0 = 0xdeadbeef;
    usop1 = 0xdeadbeef;
    if (usop0 <= usop1) {
        printf("[+] Unsigned %u <= %u\n", usop0, usop1);
    } else {
        printf("[-] Unsigned %u !<= %u\n", usop0, usop1);
        res = 1;
    }


    /* signed tests */

    sop0 = sop1 = 0;
    if (sop0 == sop1) {
        printf("[+] Signed %d == %d\n", sop0, sop1);
    } else {
        printf("[-] Signed %d != %d\n", sop0, sop1);
    }

    sop0 = sop1 = -1;
    if (sop0 == sop1) {
        printf("[+] Signed %d == %d\n", sop0, sop1);
    } else {
        printf("[-] Signed %d != %d\n", sop0, sop1);
    }

    sop0 = 0;
    sop1 = -1;
    if (sop0 > sop1) {
        printf("[+] Signed %d > %d\n", sop0, sop1);
    } else {
        printf("[-] Signed %d !> %d\n", sop0, sop1);
        res = 1;
    }

    sop0 = 2;
    sop1 = 1;
    if (sop0 > sop1) {
        printf("[+] Signed %d > %d\n", sop0, sop1);
    } else {
        printf("[-] Signed %d !> %d\n", sop0, sop1);
        res = 1;
    }

    sop0 = -2;
    sop1 = -3;
    if (sop0 > sop1) {
        printf("[+] Signed %d > %d\n", sop0, sop1);
    } else {
        printf("[-] Signed %d !> %d\n", sop0, sop1);
        res = 1;
    }

    sop0 = -1;
    sop1 = 0;
    if (sop0 < sop1) {
        printf("[+] Signed %d < %d\n", sop0, sop1);
    } else {
        printf("[-] Signed %d !< %d\n", sop0, sop1);
        res = 1;
    }

    sop0 = 1;
    sop1 = 2;
    if (sop0 < sop1) {
        printf("[+] Signed %d < %d\n", sop0, sop1);
    } else {
        printf("[-] Signed %d !< %d\n", sop0, sop1);
        res = 1;
    }

    sop0 = -3;
    sop1 = -2;
    if (sop0 < sop1) {
        printf("[+] Signed %d < %d\n", sop0, sop1);
    } else {
        printf("[-] Signed %d !< %d\n", sop0, sop1);
        res = 1;
    }

    sop0 = 1;
    sop1 = 0;
    if (sop0 >= sop1) {
        printf("[+] Signed %d >= %d\n", sop0, sop1);
    } else {
        printf("[-] Signed %d !>= %d\n", sop0, sop1);
        res = 1;
    }

    sop0 = 0;
    sop1 = 0;
    if (sop0 >= sop1) {
        printf("[+] Signed %d >= %d\n", sop0, sop1);
    } else {
        printf("[-] Signed %d !>= %d\n", sop0, sop1);
        res = 1;
    }

#define PNG_FILTER_VALUE_NONE 0
#define PNG_FILTER_VALUE_LAST 5

    unsigned int i = 0;
    for (i = 0; i <= 256; ++i) {
        unsigned char x = i;
        if (x > PNG_FILTER_VALUE_NONE) {
            printf("%d %d ", i, x);
            if (x < PNG_FILTER_VALUE_LAST)
                printf("good\n");
            else
                printf("bad\n");
        } else {
            ;//printf("good\n");
        }

    }

    return res;
}