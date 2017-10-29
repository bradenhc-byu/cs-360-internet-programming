#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

/**
 * This example extends fork1 to show how a parent
 * can use kill() to send a SIGINT to a child that is
 * infinitely looping, to terminate the child.
 *
 * In this case we rely on the default behavior of SIGINT,
 * the same signal that is sent when you press Ctrl+C in a
 * shell, which has the default action of terminating the
 * receiving process.
 */

int main() {
	int status;
	pid_t pid = fork();
	if (pid < 0) {
		printf("Fork failed!\n");
	}
	else if (pid == 0) {
		printf("I am the child and my PID is %d\n", getpid());
		while (1) {
			printf("I the child and I like infinite loops\n");
			sleep(1);
		}
		_exit(EXIT_FAILURE);
	}
	else {
		int counter = 0;
		fprintf(stderr,"I am the parent and my child's PID is %d\n", pid);
		while (waitpid(-1, &status, WNOHANG) == 0) {
			counter++;
			fprintf(stderr,"Waiting for my kid to die...\n");
			sleep(1);
			fprintf(stderr,"Sending a SIGINT to my kid\n");
			/* Instead of merely waiting for the child to terminate
			 * we will use kill(pid, SIGINT), which sends SIGINT to
			 * pid (which is the child process ID)
			 *
			 * Note that the placement of this line is for demonstration
			 * only, and is pretty inefficient all things considered.*/
			kill(pid, SIGINT);
		}
		fprintf(stderr,"My kid died, became a zombie, and I reaped it\n");
		fprintf(stderr,"These are dark times...\n");
		/* Since we're killing our child with a signal, these next few lines will
		 * actually be printed, unlike in our fork1 example */ 
		if (WIFSIGNALED(status)) {
			int signum = WTERMSIG(status);
			fprintf(stderr,"\tIt was murdered by signal %d (%s)\n", signum, strsignal(signum));
		}
		fprintf(stderr,"\tIts exit status was %d\n", (WEXITSTATUS(status)));
	}
}
