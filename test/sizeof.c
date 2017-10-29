#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_BUFFER_SIZE 1024

void func(char* array);
void func2(char* array);


int main(){
    

    printf("REFERENCE SIZES\n\n");

    printf("char\t%d\n",sizeof(char));
    printf("short\t%d\n",sizeof(short));
    printf("int\t%d\n",sizeof(int));
    printf("long\t%d\n",sizeof(long));


    unsigned char array[MAX_BUFFER_SIZE];

    printf("Size of array: %d\n",sizeof(array));
    printf("Size of array strlen: %d\n",strlen(array));

    unsigned char* array_ptr = array;

    printf("Size of array pointer: %d\n",sizeof(array_ptr));
    
    func(array);

    printf("\nFilling the array with 'The quick brown fox jumped over the lazy dog.'\n\n");

    char* str = "The quick brown fox jumped over the lazy dog.";

    strncpy(array,str,strlen(str));

    printf("Size of array with text: %d\n",sizeof(array));

    func2(array);

    printf("Content of array = %s\n",array);

    printf("Initializing new array with char data....\n");

    unsigned char* new_array[] = {0x11,0x22,0x33,0x44,0x55};

    size_t new_len = sizeof(new_array) / 8;

    printf("Size of newly initialized char array: %d\n",new_len);
    
    printf("Size of newly initialized char array strlen: %d\n",strlen(new_array));
    
    return 0;

}

void func(char* array){
    
    printf("Size of array passed to function: %d\n",sizeof(array));
    printf("Size of dereferenced array inside function: %d\n",sizeof(*array));

    return;

}

void func2(char* array){

    printf("Size of array passed to function containing text: %d\n",sizeof(array));
    printf("Size of dereferenced array inside function: %d\n",sizeof(*array));
    printf("Size of array inside function strlen: %d\n",strlen(array));
    return;

}
