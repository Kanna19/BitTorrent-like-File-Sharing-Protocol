#include <iostream>
#include <string>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sstream>
#include "helper_functions.h"
#include "torrent_parser.h"

const int CLIENT_QUEUED_LIMIT = 5;
const int listenPort = 6162;
bool finishedDownloading = false;

std::string contactTracker(char*);

void* uploadThread(void*);

char* getPieceData(char*, int, int);

int main(int argc, char* argv[])
{
    // Check if arguments are valid
    if(argc < 2)
    {
        printf("Usage: %s <Torrent Filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const int UPLOADER_COUNT = 2;

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
    for(int i = 0; i < UPLOADER_COUNT; i++) {
        pthread_create(&uploadThreadID[i], NULL, uploadThread, (void*)&listenSocket);
    }

    // Contact Tracker
    std::string trackerResponse = contactTracker(argv[1]);
    printf("%s\n", trackerResponse.c_str());


    return 0;
}

std::string contactTracker(char* torrentfile)
{
    // Parse .torrent file
    TorrentParser torrentParser(torrentfile);

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
    // Change to listenPortConcDS@54
    trackerRequest += "i" + std::to_string(torrentParser.trackerPort) + "e";

    int requestLen = trackerRequest.size();
    if(sendAll(sockFD, trackerRequest.c_str(), requestLen) == -1)
    {
        perror("sendall() failed");
        printf("Only sent %d bytes\n", requestLen);
    }

    char trackerResponse[BUFF_SIZE];
    memset(trackerResponse, 0, BUFF_SIZE);

    int responseLen = recv(sockFD, trackerResponse, BUFF_SIZE, 0);
    // Receive failed for some reason
    if(responseLen < 0)
    {
        perror("recv() failed");
        closeSocket(sockFD);
        return "";
    }

    // Connection closed by client
    if(responseLen == 0)
    {
        printf("Connection closed from client side\n");
        closeSocket(sockFD);
        return "";
    }

    closeSocket(sockFD);
    return trackerResponse;
}

void* uploadThread(void* arg) {
    printf("Uploader Thread %lu created\n", pthread_self());
    int listenSocket = *(static_cast <int*> (arg));

    while(!finishedDownloading) {
        sockaddr_in clientAddr;
        unsigned clientLen = sizeof(clientAddr);
        memset(&clientAddr, 0, clientLen);

        // Accept one client request
        int clientSocket = accept(listenSocket,(sockaddr*) &clientAddr, (socklen_t*)&clientLen);

        char clientRequest[BUFF_SIZE];
        memset(clientRequest, 0, BUFF_SIZE);

        int requestMsgLen = recv(clientSocket, clientRequest, BUFF_SIZE, 0);

        if(requestMsgLen < 0)
        {
            perror("recv() failed");
            closeSocket(clientSocket);
            continue;
        }

        // Connection closed by client
        if(requestMsgLen == 0)
        {
            printf("Connection closed from client side\n");
            closeSocket(clientSocket);
            continue;
        }

        // Check Request Type
        //If Pieces Info

        // If Parse Request For Piece
        // requestPiece is 0 if piece not there, use bencoding?
        int requestPieceNumber = 3; //getPieceNumber(clientRequest)

        if(requestPieceNumber) {
            // Function to get Data, getPieceData(torrentFilename, requestPieceNumber)
            char* requestPieceData = "";
            int pieceLength = 10;
            // strlen or default size?
            sendAll(clientSocket,requestPieceData,pieceLength);

            free(requestPieceData);
        }

        closeSocket(clientSocket);
    }
}

char* getPieceData(char* torrentFilename, int requestPieceNumber, int pieceSize) {
    TorrentParser torrentParser(torrentFilename);

    FILE* fp;
    fp = fopen(torrentParser.filename.c_str(), "r");

    fseek(fp, (requestPieceNumber-1)*pieceSize, 0);

    char* pieceData;
    pieceData = (char*)malloc(sizeof(char)*pieceSize);
    memset(pieceData,0,0);

    fread(pieceData,sizeof(char),pieceSize,fp);

    return pieceData;
}
