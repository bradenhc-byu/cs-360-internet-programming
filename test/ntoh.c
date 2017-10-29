#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

int main(){
    
    unsigned char cnum[] = {0x00, 0x04, 0x01, 0x02, 0x03};
    unsigned char* cnum_ptr = cnum;

    short num1 = ntohs(cnum_ptr);
    short num2 = htons(cnum_ptr);

    printf("ntoh conversion: %d\n",num1);

    printf("hton conversion: %d\n",num2);

    short num3 = (short)(*(cnum_ptr));

    printf("cast conversion: %d\n",num3);

    short num4 = 0;
    memcpy(&num4,cnum_ptr,sizeof(short));

    printf("memcpy number: %d\n",num4);

    num4 = ntohs(num4);

    printf("memcpy conversion: %d\n",num4);

    return 0;

}
