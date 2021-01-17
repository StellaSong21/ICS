#ifndef _WRAPPER_H
#define _WRAPPER_H

#include<unistd.h>

void Execve(const char* filename,char *const argv[],char*const envp[]); /* execve wrapper */
void Sigfillset(sigset_t *set);/* sigfillset wrapper */
void Sigemptyset(sigset_t *mask); /* sigemptyset wrapper */
void Sigaddset(sigset_t *mask,int signum); /* sigaddset wrapper */
void Sigprocmask(int how, const sigset_t * set, sigset_t * oldset); /* sigprocmask wrapper */
void Kill(pid_t pid, int sig); /* kill wrapper */
void Setpgid(pid_t pid, pid_t pgid); /* setgpid wrapper */
void unix_error(char *msg);

#endif