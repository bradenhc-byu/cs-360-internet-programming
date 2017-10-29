#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(){

    int x = 3;

    if(fork() != 0){
        printf("x=%d\n",++x);
    }

    printf("x=%d\n",--x);
    exit(0);

}
