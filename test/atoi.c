#include <stdlib.h>
#include <stdio.h>
#include <string.h>



int main(){
    int val;
    unsigned char str[] = {
        0x03, 0x77, 0x77, 0x77,
        0x07, 0x65, 0x78, 0x61,
        0x6d, 0x70, 0x6c, 0x65,
        0x03, 0x63, 0x6f, 0x6d,
        0x00
    };
    unsigned char* str_ptr = str;
    
    char c = str[0];

    val = (int)c;

    printf("String: %s\nInteger: %d\n",str,val);

    return 0;
}


