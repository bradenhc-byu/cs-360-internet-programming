#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]){

    if(argc != 2){
        printf("Usage: %s <integer>\n",argv[0]);
        exit(0);
    }

    int n = atoi(argv[1]);
    int i;
    
    for(i = 0; i < n; i++){
        fork();
    }

    printf("Hello\n");
    exit(0);

}
