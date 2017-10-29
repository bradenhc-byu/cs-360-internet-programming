#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

/**
 * This is an example of using fork and execv.
 * In this case we're launching python in the child
 * and using the parent to force the child to die
 * when we press Ctrl+C from the keyboard. Note
 * that the python interpreter usually ignores 
 * SIGINT by default.
 *
 * Also, feel free to experiment with adding parameters
 * to execv.  Just remember that the array of strings you
 * pass must be null terminated.  That is, you must 
 * terminate the array itself with a null element, not just
 * the argument stringsg individually. This is how it knows
 * when to stop reading arguments.
 */

pid_t pid;

void die(int signum) {
	if (signum == SIGINT) {
		if (pid > 0) {
			kill(pid, SIGKILL);
		}
	}
	return;
}

int main() {
	int status;
	pid = fork();
	if (pid < 0) {
		printf("Fork failed!\n");
	}
	else if (pid == 0) {
		printf("I am the child and my PID is %d\n", getpid());
		printf("I am now going to turn into python\n");
		// launch python with no arguments
		execv("/usr/bin/python", NULL);

		/*
 		 * you might add an argument like this:
 		 *
 		 *  ---------------------  First argument is always the program itself
 		 *                      V
 		 *                      V
 		 *  --------------------V-------------------------   Second argument will be a python file
 		 *                      V                        V
 		 * char* args[] = { "/usr/bin/python", "/path/to/my/program.py", NULL };
 		 * execv(args[0], args);
 		 *
 		 * assuming program.py is a python program and python is located in
 		 * /usr/bin. Note the inclusion of the NULL value as the last arg in the
 		 * array! It's important and without it your program may not work as
 		 * intended!
 		 *
 		 * You can also experiment with execve, which takes a third paramter that
 		 * lets you specify the environment variables for the program to be
 		 * launched.
 		 */
	}
	else {
		struct sigaction sigact;
		memset(&sigact, 0, sizeof(sigact));
		sigact.sa_handler = die;
		sigaction(SIGINT, &sigact, NULL);

		fprintf(stderr,"I am the parent and my child's PID is %d\n", pid);
		waitpid(-1, &status, 0);
		exit(EXIT_SUCCESS);
	}
	return 0;
}
