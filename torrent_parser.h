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

    TorrentParser(char* torrentfile = NULL)
        : trackerIP(""), trackerPort(-1), filename("")
    {
        if(torrentfile == NULL)
        {
            printf("Call constructor with non NULL parameter\n");
            return;
        }
        
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
        }

        fileIn.close();
    }
};

#endif
