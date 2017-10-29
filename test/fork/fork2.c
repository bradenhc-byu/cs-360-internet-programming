#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(){

    doit();
    printf("Hello\n");
    exit(0);

}

void doit(){

    fork();
    fork();
    printf("Hello\n");
    return;

}
