#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(){

    FILE* fp = fopen("sscanf.txt","r");
    char line[256];
    char str1[64];
    char str2[64];
    char str3[64];
    char str4[64];
    
    while(fgets(line,sizeof(line),fp)){
        sscanf(line,"%s %s %s %s",str1,str2,str3,str4);
        printf("Line Contents: %s %s %s %s\n",str1,str2,str3,str4);
    }
    
    fclose(fp);

    return 0;

}
