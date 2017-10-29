#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(){

    int i;

    for(i = 0; i < 2; i++){
        
        fork();

    }

    printf("Hello\n");
    exit(0);

}
