#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void doit();

int main(){
        
    doit();
    printf("Hello\n");
    exit(0);

}

void doit(){

    if(fork() == 0){
        fork();
        printf("Hello\n");
        exit(0);
    }
    return;

}
