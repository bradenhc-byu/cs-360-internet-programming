#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

#include <arpa/inet.h>


int main(){

    uint32_t ip_num = 584628317;
    char str[4];

    struct in_addr addr;
    addr.s_addr = ip_num;
    inet_ntop(AF_INET,&addr,str,4);

    printf("IP Address: %s\n",str);

    printf("The inet_ntoa IP address is %s\n", inet_ntoa(addr));

    return 0;

}
