#ifndef TORRENT_PARSER_H
#define TORRENT_PARSER_H

#include <iostream>
#include <string>
#include <fstream>  // ifstream ofstream
#include <sstream>

class TorrentParser
{
public:

    std::string trackerIP;
    int trackerPort;
    std::string filename;
    int piecelen;
    int pieces;
    int filesize;

    TorrentParser(char* torrentfile = NULL) :
        trackerIP(""), trackerPort(-1), filename(""),
        piecelen(0), pieces(0), filesize(0)
    {
        if(torrentfile != NULL)
            parse(torrentfile);
    }

    void parse(char* torrentfile)
    {
        std::ifstream fileIn(torrentfile);
        if(!fileIn.is_open())
        {
            printf("Couldn't open file %s\n", torrentfile);
            return;
        }

        std::string key, val;
        while(fileIn >> key >> val)
        {
            if(key == "announceip")
                trackerIP = val;

            else if(key == "announceport")
                trackerPort = std::stoi(val);

            else if(key == "filename")
                filename = val;

            else if(key == "piecelen")
                piecelen = std::stoi(val);

            else if(key == "pieces")
                pieces = std::stoi(val);

            else if(key == "length")
                filesize = std::stoi(val);
        }

        fileIn.close();
    }
};

#endif
