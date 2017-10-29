#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void printEncodedName(char* name);
char* concat(const char* s1, const char* s2);

int main(){

    char* str = "Hello World!";

    char* strIndex;
    for(strIndex = str; *strIndex != '\0'; strIndex++){
        printf("%c\t%02x\n",*strIndex,*strIndex);
    }

    printf("\n\n\n");

    // testing to see how to encode a name in DNS 
    str = "www.google.com";
    char str2[1024];
    strncpy(str2,str,strlen(str));
    char* cur_label;
    size_t totalLen;
    const char delim[2] = ".";
    char encoded_name[1024];
    char* encoded_name_ptr = encoded_name;

    // get the first label
    cur_label = strtok(str2,delim);

    while(cur_label != NULL){
        size_t len = strlen(cur_label);
        totalLen += len;
        char clen = (char)len;
        strncpy(encoded_name_ptr,&clen,1);
        encoded_name_ptr++;
        strncpy(encoded_name_ptr,cur_label,len);
        encoded_name_ptr += len;
        cur_label = strtok(NULL,delim);
    }
    char* c = "\0";
    memcpy(encoded_name_ptr,c,sizeof(char));
    
    printEncodedName(encoded_name);

    return 0;

}

void printEncodedName(char* name){
    for(name; *name != '\0'; name++){
        printf(" %02x ",*name);
    }
    printf("00\n");
}

char* concat(const char *s1, const char *s2)
{
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}