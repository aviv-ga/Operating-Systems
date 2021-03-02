//
// Created by cveksler on 6/12/18.
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <zconf.h>
#include <netdb.h>
#include <cstring>
#include <stdio.h>
#include <string>
#include "whatsappClient.h"

#define MAX_HOST_NAME 100

int main(int argc, char* argv[]) {
    if (argc != 2) {
        //error
    }
    char name[MAX_HOST_NAME + 1];
    int sock;
    int t;
    struct sockaddr_in sa;
    struct hostent *hp;

    //hostnet initialization
    gethostname(name, MAX_HOST_NAME);
    hp = gethostbyname(name);
    if (hp == NULL) {
        //error
    }
    //sockaddrr_in initlization
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = hp->h_addrtype;
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_port = htons((u_short)8875);
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        //error
    }
    if (connect(sock, (struct sockaddr *)&sa , sizeof(sa)) < 0) {
        //error
    }
    char buf = 'a';
    //char* buf = (char*)malloc(256*sizeof(char));
    //std::string msg("hello world");
    //strcpy(buf, msg.c_str());
    write(sock, (void*)buf, sizeof(char));
    //printf("%c\n", buf);
    while(1)
    {

    }

}

/**
if(read(sock, &buf, 1) < 0)
    {
        printf("read err");
        return -1;
    }
*/