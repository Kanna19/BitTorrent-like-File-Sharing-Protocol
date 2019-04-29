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

// Set to true if downloading the required file is done.
std::atomic<bool> finishedDownloading(false);
// Bitmap representing the pieces present
std::atomic<bool>* bitmap;
// Parser for .torrent file
TorrentParser torrentParser;

// Thread functions
void* uploadThread(void*);
void* downloadThread(void*);

char* getPieceData(char*, int, int);
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

    // Connection closed by tracker
    if(responseLen == 0)
    {
        printf("Tracker closed connection without responding\n");
        closeSocket(sockFD);
        return "";
    }

    closeSocket(sockFD);
    return trackerResponse;
}

void* uploadThread(void* arg)
{
    printf("Uploader Thread %lu created\n", pthread_self());
    int listenSocket = *(static_cast <int*> (arg));

    while(!finishedDownloading)
    {
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
            int responseLen = torrentParser.pieces;
            if(sendAll(clientSocket, peerResponse.c_str(), responseLen) != 0)
            {
                perror("sendAll() failed");
                printf("Only sent %d bytes\n", responseLen);
            }

            // Close connection
            closeSocket(clientSocket);
            continue;
        }

        /*
        // Check Request Type
        //If Pieces Info

        // If Parse Request For Piece
        // requestPiece is 0 if piece not there, use bencoding?
        int requestPieceNumber = 3; //getPieceNumber(clientRequest)

        if(requestPieceNumber)
        {
            // Function to get Data, getPieceData(torrentFilename, requestPieceNumber)
            int pieceLength = 10;
            char* requestPieceData = getPieceData(NULL, requestPieceNumber, pieceLength);
            // strlen or default size?
            sendAll(clientSocket, requestPieceData, pieceLength);

            free(requestPieceData);
        }
        */

        closeSocket(clientSocket);
    }

    return NULL;
}

char* getPieceData(char* torrentFilename, int requestPieceNumber, int pieceSize)
{
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
