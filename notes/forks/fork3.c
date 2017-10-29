#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

/*
 * This code extends fork2. In this case the parent
 * is unchanged, but the child has added a signal
 * handler to override the default behavior of SIGINT.
 *
 * That means the child will NOT exit and will infinitely
 * loop.
 *
 * Use "pkill -SIGKILL fork3" from the terminal to actually
 * kill the child.  Note that we use SIGKILL because no
 * process can block that signal or change its effect, which
 * is to terminate the receiving process (mercilessly).
 */


// Signal handlers must have this signature (but feel free to change the name)
void ignore(int signum) {
	if (signum == SIGINT) {
		/* Normally, you would never want to use printf
 		 * in a signal handler. See "man 7 signal" for
 		 * a list of functions that are safe to use
 		 * in a signal handler */
		printf("My dad just tried to kill me\n");
	}

	/* Note that after printing, we merely return. Thus we're basically
 	 * ignoring the SIGINT */
	return;
}

int main() {
	int status;
	pid_t pid = fork();
	if (pid < 0) {
		printf("Fork failed!\n");
	}
	else if (pid == 0) {
		// change signal handler
		struct sigaction sigact;
		memset(&sigact, 0, sizeof(sigact));
		sigact.sa_handler = ignore; // <-- this is where we specify what function to run
		sigaction(SIGINT, &sigact, NULL); // <-- means "handle SIGINT with settings from sigact"

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
			kill(pid, SIGINT);
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
