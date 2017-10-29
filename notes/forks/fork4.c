#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

/*
 * This code extends fork3  to have the parent
 * send SIGKILL instead of SIGINT, terminating
 * the child no matter what it does
 */


void ignore(int signum) {
	if (signum == SIGINT) {
		printf("My dad just tried to kill me\n");
	}
	return;
}

int main() {
	int status;
	pid_t pid = fork();
	if (pid < 0) {
		printf("Fork failed!\n");
	}
	else if (pid == 0) {
		struct sigaction sigact;
		memset(&sigact, 0, sizeof(sigact));
		sigact.sa_handler = ignore;
		sigaction(SIGINT, &sigact, NULL);

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
			fprintf(stderr,"Sending a SIGKILL to my kid\n");
			/* Use SIGKILL instead of SIGINT */
			kill(pid, SIGKILL);
		}
		fprintf(stderr,"My kid died, became a zombie, and I reaped it\n");
		fprintf(stderr,"These are dark times...\n");
		if (WIFSIGNALED(status)) {
			int signum = WTERMSIG(status);
			fprintf(stderr,"\tIt was murdered by signal %d (%s)\n", signum, strsignal(signum));
		}
		fprintf(stderr,"\tIts exit status was %d\n", (WEXITSTATUS(status)));
	}
}
