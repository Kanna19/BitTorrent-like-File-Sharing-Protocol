#include <iostream>         // Basic IO
#include <stdlib.h>         // exit()
#include <sys/types.h>      // socket()
#include <sys/socket.h>     // socket()
#include "helper_functions.h"
#include <pthread.h>
#include <atomic>
#include <map>
#include <vector>
#include "bencode_parser.h"

void* serverWorker(void*);
std::atomic <bool> terminateAllThreads;
// Filename -> Vector (IP, Port)
std::map < std::string, std::vector <std::pair<std::string, int>> > mapping;

int main(int argc, char* argv[])
{
    // Check if arguments are valid
    if(argc < 2)
    {
        printf("Usage: %s <Port Number>\n", argv[0]);
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

    terminateAllThreads = false;
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

    while(!terminateAllThreads)
    {
        sockaddr_in clientAddr;
        unsigned clientLen = sizeof(clientAddr);
        memset(&clientAddr, 0, clientLen);

        // Accept one client request
        int sockFD = accept(listeningSocketFD,
                            (sockaddr*) &clientAddr,
                            (socklen_t*)&clientLen);
        if(sockFD < 0)
        {
            perror("Accept failed");
            closeSocket(listeningSocketFD);
            printf("Terminating thread %lu\n", pthread_self());
            terminateAllThreads = true;
            pthread_exit(NULL);
        }

        char trackerRequest[BUFF_SIZE];
        memset(trackerRequest, 0, BUFF_SIZE);

        // Receive Tracker Request from client
        int requestMsgLen = recv(sockFD, trackerRequest, BUFF_SIZE, 0);

        // Receive failed for some reason
        if(requestMsgLen < 0)
        {
            perror("recv() failed");
            closeSocket(sockFD);
            continue;
        }

        // Connection closed by client
        if(requestMsgLen == 0)
        {
            printf("Connection closed from client side\n");
            closeSocket(sockFD);
            continue;
        }

        // Debug
        printf("%s\n", trackerRequest);
        BencodeParser bencodeParser(trackerRequest);
        bencodeParser.print_details();
        
        std::string trackerResponse = "ok";
        int responseLen = trackerResponse.size();

        if(sendAll(sockFD, trackerResponse.c_str(), responseLen) != 0)
        {
            perror("sendall() failed");
            printf("Only sent %d bytes\n", responseLen);
        }

        // Serve client here
        closeSocket(sockFD);
    }

    return NULL;
}
