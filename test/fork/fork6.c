#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int counter = 1;

int main(){

    if(fork() == 0){
        counter--;
        exit(0);
    }
    else{
        
        wait(NULL);
        printf("counter=%d\n",++counter);
    
    }

    exit(0);

}
