#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(){

    int pow1 = 2 ^ 2;
    int pow2 = pow(2,2);

    printf("Power test: 2 ^ 2 = %d\n",pow1);
    printf("Power test2: pow(2,2) = %d\n",pow2);

    return 0;

}

