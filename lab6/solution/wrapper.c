#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "wrapper.h"

void Execve(const char* filename,char *const argv[],char*const envp[]){
	if(execve(filename, argv, envp) < 0){ /* execve failed, then exit */
		printf("%s: Command not found\n", argv[0]);
		exit(0);
	}
}

void Sigfillset(sigset_t *set){
	if(sigfillset(set) < 0){ /* if sigfillset failed, print error message */
		unix_error("sigfillset error");
	}
}

void Sigemptyset(sigset_t *set){
	if(sigemptyset(set) < 0){ /* if sigemptyset failed, print error message */
		unix_error("sigemptyset error");
	}
}
void Sigaddset(sigset_t *set,int signum){
	if(sigaddset(set, signum) < 0){ /* if sigsddset failed, print error message */
		unix_error("sigaddset error");
	}
}

void Sigprocmask(int how, const sigset_t * set, sigset_t * oldset){
	if(sigprocmask(how, set, oldset) < 0){ /* if sigprocmask failed, print error message */
		unix_error("sigprocset error");
	}
}

void Kill(pid_t pid, int sig){
	if(kill(pid, sig) < 0){ /* if kill failed, print error message */
		unix_error("kill error");
	}
}

void Setpgid(pid_t pid, pid_t pgid){
	if(setpgid(0, 0) < 0){ /* if setgpid failed, print error message */
		unix_error("setpgid error");
	}
}
