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
#include <algorithm>

// TODO: periodically remove dead peers

// Type alias for vector of ip, port pairs
using ClientList = std::vector <std::pair<std::string, int>>;
// Set to true if termination of threads is needed
std::atomic <bool> terminateAllThreads;
// Filename -> Vector (IP, Port)
std::map < std::string, ClientList > mapping;
// Mutex for mapping
pthread_mutex_t mappingMutex;

// Function each thread executes
void* serverWorker(void*);
// Add the client details to mapping
void addToMapping(std::string, std::string, int);
// Get list of clients having this file
ClientList getClients(std::string);

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

        std::string trackerRequest = recvAll(sockFD);
        if(trackerRequest == "")
            continue;

        // Debug
        printf("%s\n", trackerRequest.c_str());
        BencodeParser bencodeParser(trackerRequest);
        bencodeParser.print_details();

        std::string trackerResponse = "";
        ClientList clientList = getClients(bencodeParser.filename);
        for(auto client : clientList)
        {
            trackerResponse += "2:ip";
            trackerResponse += std::to_string(client.first.size()) + ":";
            trackerResponse += client.first;
            trackerResponse += "8:peerport";
            trackerResponse += "i" + std::to_string(client.second) + "e";
        }

        if(trackerResponse == "")
            trackerResponse = "empty";

        int responseLen = trackerResponse.size();

        if(sendAll(sockFD, trackerResponse.c_str(), responseLen) != 0)
        {
            perror("sendall() failed");
            printf("Only sent %d bytes\n", responseLen);
        }

        // Add this client to mapping
        addToMapping(bencodeParser.filename, inet_ntoa(clientAddr.sin_addr), bencodeParser.port);

        // Serve client here
        closeSocket(sockFD);
    }

    return NULL;
}

void addToMapping(std::string filename, std::string ip, int port)
{
    // Add this client to the mapping
    pthread_mutex_lock(&mappingMutex);
    if(std::find(mapping[filename].begin(), mapping[filename].end(), make_pair(ip, port)) == mapping[filename].end())
        mapping[filename].push_back({ip, port});
    pthread_mutex_unlock(&mappingMutex);
}

ClientList getClients(std::string filename)
{
    pthread_mutex_lock(&mappingMutex);
    ClientList retVal(mapping[filename]);
    pthread_mutex_unlock(&mappingMutex);
    return retVal;
}
