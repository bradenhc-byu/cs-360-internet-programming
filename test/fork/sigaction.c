#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

void myhandler();

void myhandler(int signum){

    if(signum == SIGINT){
        printf("You just tried to kill me!!\n");
    }
    return;

}


int main(){

    int status;
    pid_t pid;
    
    pid = fork();
    if(pid < 0){
         printf("Fork failed!\n");
    }
    else if(pid == 0){
        // we are in the child process
        // change the signal handler
        struct sigaction sigact;
        memset(&sigact,0,sizeof(sigact));
        sigact.sa_handler = myhandler;
        sigaction(SIGINT, &sigact, NULL);
        
        while(1){
            printf("Around and around we go in an infinite loop, and there's nothing you can do to stop me!\n");
            sleep(2);
        }

        _exit(EXIT_FAILURE);
    }
    else{
        int counter = 0;
        printf("I am the parent and my child's PID is %d\n", pid);
        while(waitpid(-1,&status,WNOHANG) == 0){
            counter++;
            printf("Waiting for my child to die...\n");
            sleep(1);
            printf("Sending SIGINT...\n");
            kill(pid,SIGINT);
        }
        printf("My kid died...\n");
        if(WIFSIGNALED(status)){
            int signum = WTERMSIG(status);
            printf("\tIt was killed my signal %d (%s)\n",signum, strsignal(signum));
        }
        printf("It's exit status was %d\n",WEXITSTATUS(status));
    }

    exit(0);

}
