#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(){

    unsigned char array[16];
    char* array_ptr = array;
    char rand_num1 = (char) rand();
    char rand_num2 = (char) rand();
    char char1 = 'A';
    char char2 = 'A';
    short short_num = 0x6565;

    memcpy(array,&short_num,sizeof(short));

    printf("Array Contents Hex: %s\n",array);

    printf("Now adding using char\n");

    memcpy(array_ptr,&char1,sizeof(char));
    array_ptr++;
    memcpy(array_ptr,&char2,sizeof(char));

    printf("Array Contents Hex: %s\n",array);

    return 0;

}
