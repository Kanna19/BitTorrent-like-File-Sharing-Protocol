#include <iostream>         // Basic IO
#include <stdlib.h>         // exit()
#include <sys/types.h>      // socket()
#include <sys/socket.h>     // socket()
#include "helper_functions.h"

int main(int argc, char* argv[])
{
    if(argc < 2)
    {
        printf("Usage %s <Port Number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get port number from command line
    short listenPort = atoi(argv[1]);
    // Number of clients which can queue up
    const int QUEUED_LIMIT = 10;

    // Create listening socket and set to listen
    int listeningSocketFD = createTCPSocket();
    bindToPort(listeningSocketFD, listenPort);
    if(listen(listeningSocketFD, QUEUED_LIMIT) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    closeSocket(listeningSocketFD);
    return 0;
}
