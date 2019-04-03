#include <iostream>         // Basic IO
#include <stdlib.h>         // exit()
#include <sys/types.h>      // socket()
#include <sys/socket.h>     // socket()
#include "helper_functions.h"
#include <pthread.h>

void* serverWorker(void*);

int main(int argc, char* argv[])
{
    // Check if arguments are valid
    if(argc < 2)
    {
        printf("Usage %s <Port Number>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get port number from command line
    short listenPort = atoi(argv[1]);
    // Number of clients which can queue up
    const int QUEUED_LIMIT = 10;
    // Number of server threads
    const int THREAD_COUNT = 4;

    // Create listening socket and set to listen
    int listeningSocketFD = createTCPSocket();
    bindToPort(listeningSocketFD, listenPort);
    if(listen(listeningSocketFD, QUEUED_LIMIT) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    pthread_t threadID[THREAD_COUNT];
    for(int i = 0; i < THREAD_COUNT; i++)
        pthread_create(&threadID[i], NULL, serverWorker, (void*)&listeningSocketFD);

    for(int i = 0; i < THREAD_COUNT; i++)
        pthread_join(threadID[i], NULL);

    closeSocket(listeningSocketFD);
    return 0;
}

void* serverWorker(void* arg)
{
    printf("Server Thread %lu created\n", pthread_self());
    int listeningSocketFD = *(static_cast <int*> (arg));

    while(true)
    {
        sockaddr_in clientAddr;
        unsigned clientLen = sizeof(clientAddr);
        memset(&clientAddr, 0, clientLen);

        int sockFD = accept(listeningSocketFD,
                            (sockaddr*) &clientAddr,
                            (socklen_t*)&clientLen);
        if(sockFD < 0)
        {
            perror("Accept failed");
            closeSocket(listeningSocketFD);
            pthread_exit(NULL);
        }

        // Serve client here
        closeSocket(sockFD);
    }

    return NULL;
}
