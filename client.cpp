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
#include <map>
#include <cassert>

// Constants
const int UPLOADER_COUNT = 0;
const int DOWNLOADER_COUNT = 1;
const int CLIENT_QUEUED_LIMIT = 5;
const int listenPort = 6162;

// Set to true if downloading the required file is done.
std::atomic<bool> finishedDownloading(false);
// Bitmap representing the pieces present
std::atomic<bool>* bitmap;
// Parser for .torrent file
TorrentParser torrentParser;

// (IP, Port) -> Array of boolean vals
pthread_mutex_t availPiecesMutex;
std::map < std::pair <std::string, int>, std::vector <bool> > availPieces;

// Thread functions
void* uploadThread(void*);
void* downloadThread(void*);

void updateAvailablePieces(std::string);

char* getPieceData(char*, int, int);
std::string contactTracker();

std::pair <std::pair <std::string, int>, int> getPieceReq();

int main(int argc, char* argv[])
{
    // Check if arguments are valid
    if(argc < 2)
    {
        printf("Usage: %s <Torrent Filename>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

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
    // printf("%s\n", trackerResponse.c_str());
    updateAvailablePieces(trackerResponse);

    // Create download threads
    pthread_t downloadThreadID[DOWNLOADER_COUNT];
    for(int i = 0; i < DOWNLOADER_COUNT; i++)
        pthread_create(&downloadThreadID[i], NULL, downloadThread, NULL);

    // Wait for download threads to terminate
    for(int i = 0; i < DOWNLOADER_COUNT; i++)
        pthread_join(downloadThreadID[i], NULL);

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
    // Change to listenPortConcDS@54
    trackerRequest += "i" + std::to_string(listenPort) + "e";

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

void* downloadThread(void* arg)
{
    printf("Downloader Thread %lu created\n", pthread_self());

    while(true)
    {
        std::pair <std::pair <std::string, int>, int> request = getPieceReq();

        // Peer address
        sockaddr_in peerAddr;
        unsigned peerAddrLen = sizeof(peerAddr);
        memset(&peerAddr, 0, peerAddrLen);
        peerAddr.sin_family = AF_INET;
        peerAddr.sin_addr.s_addr = inet_addr(request.first.first.c_str());
        peerAddr.sin_port = htons(request.first.second);

        // Create a socket and connect to the peer
        int sockFD = createTCPSocket();
        int connectRetVal = connect(sockFD, (sockaddr*) &peerAddr, peerAddrLen);
        if(connectRetVal < 0)
        {
            perror("connect() to peer");
            closeSocket(sockFD);
            exit(EXIT_FAILURE);
        }

        // Send request to connected peer
        printf("Connected to peer %s:%d\n", request.first.first.c_str(), request.first.second);

        std::string peerRequest = "";
        peerRequest += "5:piece";
        peerRequest += "i" + std::to_string(request.second) + "e";
        printf("Peer req: %s\n", peerRequest.c_str());

        int requestLen = peerRequest.size();
        if(sendAll(sockFD, peerRequest.c_str(), requestLen) != 0)
        {
            perror("sendAll() failed");
            printf("Only sent %d bytes\n", requestLen);
        }

        std::string peerResponse = recvAll(sockFD);
        if(peerResponse.size() == 0)
            continue;

        printf("Received piece from peer\n");
        closeSocket(sockFD);
        return NULL;
    }

    return NULL;
}

std::pair <std::pair <std::string, int>, int> getPieceReq()
{
    // Take a snapshot of all pieces available
    pthread_mutex_lock(&availPiecesMutex);
    std::map < std::pair <std::string, int>, std::vector <bool> > avail = availPieces;
    pthread_mutex_unlock(&availPiecesMutex);

    // Decide whom to ask what
    // Change this part to the appropriate algo req
    auto it = avail.begin();
    std::advance(it, rand() % avail.size());
    std::vector<bool> v = it->second;
    return {it->first, rand() % torrentParser.pieces};
}

void updateAvailablePieces(std::string trackerResponse)
{
    printf("%s\n", trackerResponse.c_str());

    // Parse the tracker response
    BencodeParser bencodeParser(trackerResponse);

    // Peer address
    sockaddr_in peerAddr;
    unsigned peerAddrLen = sizeof(peerAddr);

    for(int i = 0; i < (int)bencodeParser.peer_ip.size(); i++)
    {
        memset(&peerAddr, 0, peerAddrLen);
        peerAddr.sin_family = AF_INET;
        peerAddr.sin_addr.s_addr = inet_addr(bencodeParser.peer_ip[i].c_str());
        peerAddr.sin_port = htons(bencodeParser.peer_port[i]);

        // Create a socket and connect to the peer
        int sockFD = createTCPSocket();
        int connectRetVal = connect(sockFD, (sockaddr*) &peerAddr, peerAddrLen);
        if(connectRetVal < 0)
        {
            perror("connect() to peer");
            closeSocket(sockFD);
            exit(EXIT_FAILURE);
        }

        // Send request to connected peer
        printf("Connected to peer %s:%d\n", bencodeParser.peer_ip[0].c_str(), bencodeParser.peer_port[0]);
        std::string peerRequest = "";
        peerRequest += "8:bitfield";
        peerRequest += "i1e";

        int requestLen = peerRequest.size();
        if(sendAll(sockFD, peerRequest.c_str(), requestLen) != 0)
        {
            perror("sendAll() failed");
            printf("Only sent %d bytes\n", requestLen);
        }

        // Get response from connected peer
        std::string peerResponse = recvAll(sockFD);
        if(peerResponse == "")
            return;
        closeSocket(sockFD);

        printf("Peer response: %s\n", peerResponse.c_str());
        assert((int)peerResponse.size() == torrentParser.pieces);

        std::vector<bool> v(torrentParser.pieces);
        for(int i = 0; i < torrentParser.pieces; i++)
            v[i] = (peerResponse[i] - '0');

        // Update available pieces
        pthread_mutex_lock(&availPiecesMutex);
        availPieces[{bencodeParser.peer_ip[i], bencodeParser.peer_port[i]}] = v;
        pthread_mutex_unlock(&availPiecesMutex);
    }
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

        std::string clientRequest = recvAll(clientSocket);
        if(clientRequest == "")
            continue;

        // Parse the client request
        BencodeParser bencodeParser(clientRequest);
        bencodeParser.print_details();

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
