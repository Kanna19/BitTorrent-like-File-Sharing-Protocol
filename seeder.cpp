#include <iostream>
#include <string>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sstream>
#include <atomic>
#include "helper_functions.h"
#include "torrent_parser.h"
#include "bencode_parser.h"
#include <vector>

// Constants
const int UPLOADER_COUNT = 2;
const int DOWNLOADER_COUNT = 2;
const int CLIENT_QUEUED_LIMIT = 5;
int listenPort;

// Bitmap representing the pieces present
std::atomic<bool>* bitmap;
// Parser for .torrent file
TorrentParser torrentParser;

// Thread function
void* uploadThread(void*);

std::string getPieceData(int, int);
std::string contactTracker();

int main(int argc, char* argv[])
{
    // Check if arguments are valid
    if(argc < 3)
    {
        printf("Usage: %s <Torrent Filename> <Listen Port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get listen port from command line arguments
    listenPort = atoi(argv[2]);

    // Parse .torrent file
    torrentParser.parse(argv[1]);

    // Initialize bitmap
    bitmap = new std::atomic <bool> [torrentParser.pieces];
    for(int i = 0; i < torrentParser.pieces; i++)
        bitmap[i] = true;

    // Create Listen Socket for upload threads to use
    int listenSocket = createTCPSocket();
    bindToPort(listenSocket, listenPort);

    if(listen(listenSocket, CLIENT_QUEUED_LIMIT) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Create upload threads
    pthread_t uploadThreadID[UPLOADER_COUNT];
    for(int i = 0; i < UPLOADER_COUNT; i++)
        pthread_create(&uploadThreadID[i], NULL, uploadThread, (void*)&listenSocket);

    // Contact Tracker
    std::string trackerResponse = contactTracker();
    printf("%s\n", trackerResponse.c_str());

    // Wait for upload threads to terminate
    for(int i = 0; i < UPLOADER_COUNT; i++)
        pthread_join(uploadThreadID[i], NULL);

    return 0;
}

std::string contactTracker()
{
    // Server address
    sockaddr_in serverAddr;
    unsigned serverAddrLen = sizeof(serverAddr);
    memset(&serverAddr, 0, serverAddrLen);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(torrentParser.trackerIP.c_str());
    serverAddr.sin_port = htons(torrentParser.trackerPort);

    // Create a socket and connect to tracker
    int sockFD = createTCPSocket();
    int connectRetVal = connect(sockFD, (sockaddr*) &serverAddr, serverAddrLen);
    if(connectRetVal < 0)
    {
        perror("connect() to tracker");
        closeSocket(sockFD);
        exit(EXIT_FAILURE);
    }

    printf("Connected to tracker\n");

    std::string trackerRequest = "8:filename";
    trackerRequest += std::to_string(torrentParser.filename.size()) + ":" + torrentParser.filename;

    trackerRequest += "4:port";
    trackerRequest += "i" + std::to_string(listenPort) + "e";

    // Set seeder field
    trackerRequest += "6:seeder";
    trackerRequest += "i1e";

    int requestLen = trackerRequest.size();
    if(sendAll(sockFD, trackerRequest.c_str(), requestLen) == -1)
    {
        perror("sendall() failed");
        printf("Only sent %d bytes\n", requestLen);
    }

    std::string trackerResponse = recvAll(sockFD);
    closeSocket(sockFD);
    return trackerResponse;
}

void* uploadThread(void* arg)
{
    printf("Uploader Thread %lu created\n", pthread_self());
    int listenSocket = *(static_cast <int*> (arg));

    while(true)
    {
        sockaddr_in clientAddr;
        unsigned clientLen = sizeof(clientAddr);
        memset(&clientAddr, 0, clientLen);

        // Accept one client request
        int clientSocket = accept(listenSocket,(sockaddr*) &clientAddr, (socklen_t*)&clientLen);

        std::string clientRequest = recvAll(clientSocket);
        if(clientRequest == "")
            continue;

        // Parse the client request
        BencodeParser bencodeParser(clientRequest);
        // bencodeParser.print_details();

        // Check if bitmap was requested by the client
        if(bencodeParser.bitfieldRequested)
        {
            // Convert bitmap to str
            std::string peerResponse = "";
            for(int i = 0; i < torrentParser.pieces; i++)
                peerResponse += std::to_string(bitmap[i]);

            // Send bitmap
            int responseLen = peerResponse.size();
            if(sendAll(clientSocket, peerResponse.c_str(), responseLen) != 0)
            {
                perror("sendAll() failed");
                printf("Only sent %d bytes\n", responseLen);
            }

            // Close connection
            closeSocket(clientSocket);
            continue;
        }

        // As this is a seeder no checking if piece is present needed
        int requestPieceNumber = bencodeParser.pieceRequest;
        printf("Requested piece: %d\n", requestPieceNumber);
        std::string requestPieceData = getPieceData(requestPieceNumber, torrentParser.piecelen);
        int responseLen = requestPieceData.size();
        if(sendAll(clientSocket, requestPieceData.c_str(), responseLen) != 0)
        {
            perror("sendAll() failed");
            printf("Only sent %d bytes\n", responseLen);
        }

        closeSocket(clientSocket);
    }

    return NULL;
}

std::string getPieceData(int requestPieceNumber, int pieceSize)
{
    FILE* fp;
    fp = fopen(torrentParser.filename.c_str(), "r");

    fseek(fp, (requestPieceNumber - 1) * pieceSize, 0);

    char* pieceData;
    pieceData = (char*)malloc(sizeof(char) * pieceSize);
    memset(pieceData, 0, pieceSize);
    fread(pieceData, sizeof(char), pieceSize, fp);

    std::string ret = pieceData;
    free(pieceData);

    return ret;
}
