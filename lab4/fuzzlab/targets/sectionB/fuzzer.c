#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>

#include <time.h>
#define MAXLINE 1024
#define MAXLEN 1024

//argv{input, output, exe}
int main(int argc,char *argv[]){
	char *random1(char *);
	char *random2(char *);
	char *random3(char *);
	char *random4(char *);
	char *random5(char *);
	char *random6(char *);
	int fuzz(char *, char *, char *, char *);
	
	int end;
	
	//文件夹
	char *queuedir = (char *) malloc(strlen(argv[2]) + strlen("/queue") + 1);
	sprintf(queuedir, "%s%s", argv[2], "/queue");
	char *crashdir = (char *) malloc(strlen(argv[2]) + strlen("/crash") + 1);
	sprintf(crashdir, "%s%s", argv[2], "/crash");
	
	int qu = mkdir(queuedir, 0777);
	int cr = mkdir(crashdir, 0777);
	
	char *dir = (char *) malloc(MAXLEN);
	char *string = (char *) malloc(MAXLEN);
	FILE *input;
	FILE *output;
	int queue = 0;
	
	srand((unsigned) time(NULL));
	while(1){
		//printf("%d\n", queue);
		input = fopen(argv[1] , "rb");
		sprintf(dir, "%s/queue/testcase%d", argv[2], queue++);
		
		output = fopen(dir, "wb");	
		while((end = fread(string, sizeof(char), MAXLEN, input)) != 0){
			int i = 0;
			while(i < 4){
				int random = rand() % 6;
				//printf("%d\n", random);
				switch(random){
					case 0:
						string = random1((char *)string);
						//printf("case0 : %s\n", string);
						break;
					case 1:
						string = random2((char *)string);
						//printf("case1 : %s\n", string);
						break;
					case 2:
						string = random3((char *)string);
						//printf("case2 : %s\n", string);
						break;
					case 3:
						string = random4((char *)string);
						//printf("case3 : %s\n", string);
						break;
					case 4:
						string = random5((char *)string);
						//printf("case5 : %s\n", string);
						break;
					case 5:
						string = random6((char *)string);
						//printf("case5 : %s\n", string);
						break;
				}
				i++;
			}
			
			fwrite(string, sizeof(char), end, output);
		}
		fclose(input);
		fclose(output);
		fuzz(dir, argv[3], crashdir, queuedir);
	}
  return 0;
}

char *random1(char *string){
	unsigned char a = rand() % 255 + 1;
	int b = rand() % (strlen(string));
	string[b] = a;
	return string;
}

char *random2(char *string){
	int a = rand() % strlen(string);
	int b = rand() % strlen(string);
	char tmp = string[a];
	string[a] = string[b];
	string[b] = tmp;
	return string;
}

char *random3(char *string){
	int a = rand() % (strlen(string));
	if(string[a] >= 0 && string[a] <= 254)
		string[a] = string[a] + 1;
	else
		string[a] = 1;
	return string;
}

char *random4(char *string){
	int a = rand() % (strlen(string));
	if(string[a] >= 1)
		string[a] = string[a]-1;
	else
		string[a] = 255;
	return string;
}

char *random5(char *string){
	unsigned char a = rand() % 255 + 1;
	int b = rand() % (strlen(string));
	for(int i = strlen(string); i >= b; i--){
		string[i + 1] = string[i];
	}
	string[b] = a;
	string[strlen(string)] = '\0';
	return string;
}

char *random6(char *string){
	int b = rand() % (strlen(string));
	for(int i = b; i < strlen(string); i++){
		string[i] = string[i + 1];
	}
	//string[strlen(string)] = '\0';
	return string;
}
//argv1 input argv2 执行
int fuzz(char *argv1, char *argv2, char *crashdir, char *queuedir){
	char *b = " 2>&1";
	char *cmd = (char *) malloc(strlen(argv2) + strlen(" ") + strlen(argv1) + strlen(b) + 1);
	sprintf(cmd, "%s%s%s%s", argv2, " ", argv1, b);	
	
	//printf("%s\n", cmd);
	
	FILE *pp;
	pp = popen(cmd, "r");
	
	char buff[MAXLINE];
	char *result = "\0";
	
	if(NULL == pp){
		printf("popen() error!\n");
		exit(1);
	}
	
	while(fgets(buff, sizeof(buff), pp) != NULL){
		if('\n' == buff[strlen(buff) - 1]){
			buff[strlen(buff) - 1] = '\0';
		}
		char *strcopy = (char *) malloc(strlen(result)+1);
    strcpy(strcopy,result);
    result = (char *) malloc(strlen(strcopy) + strlen(buff)+1);
    sprintf(result, "%s%s", strcopy, buff);
	}
	
	int end = 0;
	end = pclose(pp);
	
	if(end == -1){
		perror("Failed to close file\n");
		exit(1);
	} else {
		int status;
		if( end != 0 || WEXITSTATUS(end) != 0){
			printf("%d\n", end);
			printf("testcase:[%s] \nresult:[%s] \n", argv1, result);
			printf("command:[%s] \ncomplete status:[%d] \ncommand return:[%d] \n\n",cmd, end, WEXITSTATUS(end));
			
			/*
			printf("queuedir = %s\n", queuedir);
			printf("argv1 = %s\n", argv1);
			printf("start = %d\n", strlen(queuedir)+strlen("/"));
			//int start = strlen(queuedir) + strlen("/");
			printf("%.2f\n", strlen("/"));
			*/
			char *file = strrchr(argv1, argv1[12]);
			file = strchr(file, file[1]);
			//printf("fileName = %s\n", file);

			char *cdir = (char *) malloc(strlen(crashdir) + strlen("/") + strlen(file) + 1);
			sprintf(cdir , "%s%s%s", crashdir, "/", file);
			
			FILE *infile = fopen(argv1, "rb");
			FILE *output = fopen(cdir, "wb");
			
			char buf[MAXLEN];
			while((end = fread(buf, sizeof(char), MAXLEN, infile)) != 0){
				fwrite(buf, sizeof(char), end, output);
			}
			
			fclose(output);
			fclose(infile);
		}
	}
	return 0;
}
