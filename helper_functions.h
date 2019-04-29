#ifndef HELPER_FUNCTIONS_H
#define HELPER_FUNCTIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <string>

int createTCPSocket();
void bindToPort(int, short);
void closeSocket(int);
const int BUFF_SIZE = (1 << 15);

// Creates TCP Socket
int createTCPSocket()
{
    // Create TCP socket
    int sockFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Socket creation failed
    if(sockFD < 0)
    {
        perror("TCP socket creation");
        exit(EXIT_FAILURE);
    }

    // Socket creation successfull
    printf("Created socket %d\n", sockFD);
    return sockFD;
}

// Binds given socket to local port specified
void bindToPort(int sockFD, short port)
{
    sockaddr_in addrport;
    memset(&addrport, 0, sizeof(addrport));         // Reset

    addrport.sin_family = AF_INET;                  // IPv4
    addrport.sin_port = htons(port);                // Port number
    addrport.sin_addr.s_addr = htonl(INADDR_ANY);   // Binds to local IP

    // Bind to local port
    int status = bind(sockFD, (sockaddr*) &addrport, sizeof(addrport));
    if(status < 0)
    {
        perror("Binding to local port");
        closeSocket(sockFD);
        exit(EXIT_FAILURE);
    }

    printf("Bound socket %d to local port %d\n", sockFD, port);
}

int sendAll(int sockFD, const char* buff, int& len)
{
    int total = 0;
    int bytesLeft = len;
    int sendRetVal = 0;

    while(total < len)
    {
        sendRetVal = send(sockFD, buff + total, bytesLeft, 0);
        if(sendRetVal == -1)
            break;

        total += sendRetVal;
        bytesLeft -= sendRetVal;
    }

    len = total;
    return (sendRetVal == -1 ? -1 : 0);
}

std::string recvAll(int sockFD)
{
    char response[BUFF_SIZE];
    memset(response, 0, BUFF_SIZE);

    int responseLen = recv(sockFD, response, BUFF_SIZE, 0);

    if(responseLen < 0)
    {
        perror("recv() failed");
        closeSocket(sockFD);
        return "";
    }

    // Connection closed by client
    if(responseLen == 0)
    {
        printf("Connection closed from the other side\n");
        closeSocket(sockFD);
        return "";
    }

    std::string ret = response;
    return ret;
}

void closeSocket(int sockFD)
{
    // Close socket
    printf("Closing socket %d...\n", sockFD);
    close(sockFD);
}

#endif
