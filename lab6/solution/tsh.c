/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 * <Yijing Song 17302010079>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "wrapper.h"

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */


/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are my functions */
int getid(char *id);
void getIdStr(char *dst,char *src, int n);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/*
 * main - The shell's main routine 
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
	    break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
	    break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
	    break;
		default:
            usage();
		}
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) {

		/* Read command line */
		if (emit_prompt) {
			printf("%s", prompt);
			fflush(stdout);
		}
		if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
			app_error("fgets error");
		if (feof(stdin)) { /* End of file (ctrl-d) */
			fflush(stdout);
			exit(0);
		}

		/* Evaluate the command line */
		eval(cmdline);
		fflush(stdout);
		fflush(stdout);
    }

    exit(0); /* control never reaches here */
}
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{
	char *argv[MAXARGS];
	char buf[MAXLINE];
	int bg;
	pid_t pid;
	sigset_t mask, prev;
	
	strcpy(buf, cmdline);
	bg = parseline(buf, argv);
	if(argv[0] == NULL){
		return;
	}
	
	if(!builtin_cmd(argv)){
		Sigemptyset(&mask);
		Sigaddset(&mask, SIGCHLD); /* block SIGCHLD */
		Sigaddset(&mask, SIGINT); /* block SIGINT */
	    Sigaddset(&mask, SIGTSTP); /* block STGTSTP */
		Sigprocmask(SIG_BLOCK, &mask, &prev);
		
		if((pid = fork()) == 0){
			/* Child process */
			Sigprocmask(SIG_UNBLOCK, &mask, NULL); /* unblock SIGCHLD, SIGINT, SIGTSTP in child process */
			Setpgid(0,0); /* setpgid, for SIGINT and SIGTSTP */
			Execve(argv[0], argv, environ); /* run */
		}else if(pid > 0){
			/* Parent process */
			addjob(jobs, pid, bg?BG:FG,cmdline); /* add job before unblock SIGCHLD and SIGINT, handlers of which will need joblist jobs */
            Sigprocmask(SIG_UNBLOCK,&mask,NULL);
			
			/* Parent waits for foreground job to terminate */
			if(!bg){
				waitfg(pid); /* wait for foreground job */
			}
			else{
				printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
			}
		} else{
			/* fork failed */
			unix_error("fork error");
		}
	}
    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv) 
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
	if(!strcmp(argv[0], "quit")){ /* address "quit" */
		exit(0);
	}
	if(!strcmp(argv[0], "&")){ /* if only "&", ignore */
		return 1;
	}
	if(!strcmp(argv[0], "fg") || !strcmp(argv[0], "bg")){ /* address "fg" and "bg" */
		do_bgfg(argv);
		return 1;
	}
	if(!strcmp(argv[0], "jobs")){ /* address "jobs" */
		listjobs(jobs);
		return 1;
	}
    return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
	struct job_t *optjob; /* the job_t after bg or fg commands */
	pid_t pid;
	int jid;
	char *idStr = argv[1]; /* the argument after the command, from which we get the pid or jid of the job */
	
	/* no argument or more than two arguments*/
	if(idStr == NULL){ 
		printf("%s command requireds pid or %%jobid argument\n",argv[0]);
		return;
	}
	if(argv[2] != NULL){
		printf("%s command: %s <jid or pid>.\n",argv[0], argv[0]);
		return;
	}
	
	/* get jid or pid from argv[1], and get job by jid or pid */
	if(idStr[0] == '%'){ /* if jid */
		jid = getid(idStr);
		if(!(optjob = getjobjid(jobs, jid))){
			printf("%%%d: No such job\n",jid);
            return;
		}
	} else if(isdigit(idStr[0])){ /* if pid */
		pid = getid(idStr);
		if(!(optjob = getjobpid(jobs, pid))){
			printf("(%d): No such process\n",pid);
			return;
		}
	} else{
		printf("%s: command requires PID or %%jobid\n", argv[0]); /* if the argument is not a digit */
		return;
	}
	
	/* send SIGCONT to process in background or foreground, and change their status into BG or FG */
	if(!strcmp(argv[0],"bg")){
		Kill(-(optjob->pid), SIGCONT); /* group pid: to child process as well as its child process */
        optjob->state = BG;
        printf("[%d] (%d) %s",optjob->jid,optjob->pid,optjob->cmdline);
    } else if(!strcmp(argv[0],"fg")){
		if(fgpid(jobs) > 0){ /* promise there is only one process in foreground */
			printf("The foreground can only run at most one process.\n");
			return;
		}
		Kill(-(optjob->pid), SIGCONT);
        optjob->state = FG;
        waitfg(optjob->pid);
    }else{ /* control never reaches here */
        printf("%s error.\n",argv[0]);
    }
    return;
}

/* copy n chars from right in src to dst */
void getIdStr(char *dst,char *src, int n){
    char *p = src;  
    char *q = dst;  
    int len = strlen(src);  
    if(n>len) n = len;  
    p += (len-n);
    while((*(q++) = *(p++)));
}  

/* get jid or pid form id */
int getid(char *id){
	int rid;
	char id0 = id[0];
    int len=strlen(id);
    char *dst = malloc(sizeof(char)*len*2);
	if(id0 == '%'){ /* if '%' exits, copy len-1 */
		getIdStr(dst, id, len-1);
	}
	else{
		getIdStr(dst, id, len);	
	}
	rid = atoi(dst);
	free(dst);
	return rid;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
	while(pid == fgpid(jobs)){ /* if fgpid(jobs) == pid, while+sleep until pid is not in foreground */
        sleep(1);
    }
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
	sigset_t mask, prev;
	Sigfillset(&mask);
	Sigprocmask(SIG_SETMASK, &mask, &prev);
	int olderrno = errno;
	int status;
    pid_t pid;
	
	/* WNOHANG|WUNTRACED: Return immediately, with a return value of 0, if none of children in the wait set
	 * 		has stopped or terminated, or with a return value equal to the PID of one of the stopped or terminated
	 *		children.
	 */
    while((pid = waitpid(-1,&status,WNOHANG|WUNTRACED))>0){   /* Reap a zombie child */
        if(WIFSTOPPED(status)){ /* Return true if the child that caused the return is currently stopped */
            sigtstp_handler(-pid);
        }
        else if(WIFSIGNALED(status)){ /* the child process terminates because of a signal that was caught */
            sigint_handler(-pid);
        }
        else if(WIFEXITED(status)){/* exit or return normal */
            deletejob(jobs, pid);      /* Delete the child from the job list */
        }
    }
	// why there is a system call?
    //if(errno != ECHILD) /* out of while, error == ECHILD */
		//unix_error("waitpid error");
	errno = olderrno;
	Sigprocmask(SIG_SETMASK, &prev, NULL);
    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
	//fprintf(stderr, "test: %d\n", getpid());
	sigset_t mask, prev;
	Sigfillset(&mask);
	Sigprocmask(SIG_SETMASK, &mask, &prev);
	int olderrno = errno;
	pid_t pid=fgpid(jobs);
    int jid=pid2jid(pid);

    if(pid != 0){
        if(pid == -sig){ /* SIGCHLD: WIFSIGNALED(status) */
			//printf("chld: %d\n", getpid());
			//Kill(-pid, SIGINT); /* not need, because when child process address SIGINT from itself, it has Kill(-pid, SIGINT) */
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(-sig), -sig, SIGINT);
            deletejob(jobs, pid);
        }
        else if(sig == SIGINT){
			//control does not reaches here 
			//fprintf(stderr, "int: %d\n", getpid());
            Kill(-pid, SIGINT); /* send SIGINT to child process and its children */
            printf("Job [%d] (%d) terminated by signal %d\n", jid, pid, sig);
            deletejob(jobs, pid);
        }
    }
	errno = olderrno;
	Sigprocmask(SIG_SETMASK, &prev, NULL);
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
	sigset_t mask, prev;
	Sigfillset(&mask);
	Sigprocmask(SIG_SETMASK, &mask, &prev);
	int olderrno = errno;
	pid_t pid=fgpid(jobs);
    int jid=pid2jid(pid);
    if(pid != 0)
    {
        if(sig == SIGTSTP){
			
			printf("tstp: %d",getpid());
			
            printf("Job [%d] (%d) Stopped by signal %d\n", jid, pid, sig);
            getjobpid(jobs, pid)->state=ST;
            Kill(-pid, SIGTSTP);
        }else if(pid == -sig){ /* SIGCHLD: WIFSTOPPED(status) */
            printf("Job [%d] (%d) Stopped by signal %d\n",jid,pid,SIGTSTP);
            getjobpid(jobs,pid)->state=ST;
			//Kill(-pid, SIGTSTP); //不需要？
        }
    }
	errno = olderrno;
	Sigprocmask(SIG_SETMASK, &prev, NULL);
    return;
}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;

    if (pid < 1)
		return 0;

    for (i = 0; i < MAXJOBS; i++) {
		if (jobs[i].pid == pid) {
if(verbose){
	        printf("Deleted job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
            }
			clearjob(&jobs[i]);
			nextjid = maxjid(jobs)+1;
			return 1;
		}
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid == jid)
	    return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	return 0;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

