#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <termios.h>

/*
 * Easy Read Homework/Mini Lab
 * Braden Hitchcock
 * CS360
 * 2.27.2017
 * NetID: hitchcob
 *
 * Uses fork() and execv(), as well as various other signals,
 * to regulate and modify the usage of a given binary reader file,
 * adding controls for greater user experience
 * 
*/

// PROGRAM VARIABLES ----------------------------------------------
//

// PROGRAM FUNCTIONS ----------------------------------------------
//
int main(int argc, char* argv[]);
void printHelp();
char mygetch();
void clearScreen();




int main(int argc, char* argv[]){

    char input[256];
    char c;
    pid_t pid;
    int reading;
    
    while(1){
        // Print the controlls help on program startup
        reading = 1;
        printHelp();

        // Wait for text file input and start reader with input
        scanf("%s",input);
        char* args[] = {"./reader",input,NULL};
        pid = fork();
        if(pid == 0){ // the child process
            // start a new instance of the reader binary 
            execv(args[0], args);
        }
        else if (pid > 0){ // the parent process
            // wait for user input and send signals to child process
            while(reading){
                c = mygetch();
                switch(c){
                    
                    case 's':   // select a new book
                                kill(pid,SIGKILL);
                                clearScreen();
                                reading = 0;
                                break;

                    case 'p':   // pause the reading
                                kill(pid,SIGSTOP);
                                break;

                    case 'r':   // resume the reading
                                kill(pid,SIGCONT);
                                break;

                    case '+':   // speed up the reading
                                kill(pid,SIGUSR1);
                                break;

                    case '-':   // slow down the reading
                                kill(pid,SIGUSR2);
                                break;

                    case 'q':   // quit the program 
                                kill(pid,SIGKILL);
                                clearScreen();
                                reading = 0;
                                return 0;
                                break;
                    
                    default:    break; // handle all other inputs

                }

            }
        }
        else{
            printf("ERROR: fork failed, returning with value of 1");
            return 1;
        }
    }
    
    return 0;
}

void printHelp(){

    char* welcome = "Welcome to the speed reader controller!";
    char* beginHelp = "Use the following hot keys to control your experience:";
    char* start = "[s]tart new book";
    char* pause = "[p]ause playback";
    char* resume = "[r]esume playback";
    char* speedUp = "[+] speed up";
    char* slowDown = "[-] slow down";
    char* quit = "[q]uit";
    char* file = "Please enter the path to a text file to read:";

    printf("%s\n%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n%s\n",
            welcome,
            beginHelp,
            start,
            pause,
            resume,
            speedUp,
            slowDown,
            quit,
            file);
    return;

}


char mygetch(void){
    struct termios oldt,newt;
    int ch;
    tcgetattr( STDIN_FILENO, &oldt );
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
    return ch;
}

void clearScreen(){
  const char* CLEAR_SCREE_ANSI = "\e[1;1H\e[2J";
  write(STDOUT_FILENO,CLEAR_SCREE_ANSI,12);
}