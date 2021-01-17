#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>

#include <time.h>

void main(){
	char *fileName = "normalsampleinput";
	FILE *file = fopen(fileName, "rb");
	char string[1024];
	int length = fread(string, sizeof(char), 1024, file);
	//printf("%s\n", string);
	unsigned char a = rand() % 1;
	printf("%s\n", '\0');
	string[3] = a;
	//mkdir("test", 0777);
	FILE *test;
	test = fopen("test", "wb");
	fwrite(string, sizeof(char), length, test);
	fclose(test);
	fclose(file);
}
