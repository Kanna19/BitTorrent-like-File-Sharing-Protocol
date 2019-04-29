#ifndef BENCODE_PARSER_H
#define BENCODE_PARSER_H

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <assert.h>
#include <vector>

class BencodeParser
{
public:
    // Add required headers
    std::string filename;
    int port;
    bool isSeeder;
    bool bitfieldRequested;
    std::vector <std::string> peer_ip;
    std::vector <int> peer_port;
    int pieceRequest;

    BencodeParser(std::string str = ""):
        filename (""), port (0), isSeeder (false),
        bitfieldRequested (false), peer_ip (),
        peer_port (), pieceRequest(-1)
    {
        if(str == "")
        {
            printf("Constructor called with empty string\n");
            return;
        }

        int i = 0;
        int len = str.size();
        while(i < len)
        {
            // Fills a key-value pair in each iteration
            int j = i;

            // Fill key
            int len = 0;
            while(str[j] != ':')
            {
                len = (len * 10) + (str[j] - '0');
                j++;
            }
            j++;    // For ':'

            std::string key = "";
            while(len > 0)
            {
                key += str[j];
                len--;
                j++;
            }

            // Fill value
            // Check if val is int
            if(str[j] == 'i')
            {
                j++;

                // Read and store integer
                int val_int = 0;
                while(str[j] != 'e')
                {
                    val_int = (val_int * 10) + (str[j] - '0');
                    j++;
                }
                j++;    // For 'e'

                // Assign val to key accordingly
                if(key == "port")
                {
                    port = val_int;
                }

                else if(key == "seeder")
                {
                    isSeeder = true;
                }

                else if(key == "bitfield")
                {
                    bitfieldRequested = true;
                }

                else if(key == "peerport")
                {
                    peer_port.push_back(val_int);
                }

                else if(key == "piece")
                {
                    pieceRequest = val_int;
                }
            }

            // val is string
            else
            {

                int len = 0;
                while(str[j] != ':')
                {
                    len = (len * 10) + (str[j] - '0');
                    j++;
                }
                j++;    // For ':'

                std::string val = "";
                while(len > 0)
                {
                    val += str[j];
                    j++;
                    len--;
                }

                // Assign val to key accordingly
                if(key == "filename")
                {
                    filename = val;
                }

                else if(key == "ip")
                {
                    peer_ip.push_back(val);
                }
            }

            i = j;
        }
    }

    void print_details()
    {
        printf("Filename: %s\n", filename.c_str());
        printf("Port: %d\n", port);
        printf("Seeder: %s\n", isSeeder ? "True" : "False");
    }
};

#endif
