//------------------------------------------------------------------------------
//
// File:    Main.cpp
// Project: omxreceiver
// Brief:   Receive and decode video TS multicasts on Raspberry Pi
//
// Copyright (c) Sam Chalmers 2014
// 
// This software is licensed under the terms of the MIT license.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//------------------------------------------------------------------------------

#include <iostream>

#include "Types.h"
#include "PesAnalyser.h"
#include "OmxVideoDecode.h"
#include "OsdGraphics.h"
#include "PictureAnalyser.h"
#include "StreamReader.h"
#include "TsAnalyser.h"

// #define ENABLE_OSD

//------------------------------------------------------------------------------
//
int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cout << "Syntax: omxreceiver <multicast IP> <port> <videoPid>" << std::endl;
        exit(1);
    }
    
    const char *multicastIP = argv[1];
    uint16_t port = atoi(argv[2]);
    uint32_t videoPid = atoi(argv[3]);
    std::cout << "Using multicast IP=" << argv[1] << ", port=" << argv[2];
    std::cout << ", videoPid=" << argv[3] << std::endl;
        
    StreamReader streamReader;
    TsAnalyser tsAnalyser(streamReader);
    PesAnalyser pesAnalyser(tsAnalyser, videoPid);
    OsdGraphics osdGraphics;
    PictureAnalyser pictureAnalyser;

    OmxVideoDecode *omxVideoDecode = new OmxVideoDecode();
    if (omxVideoDecode)
    {
        try
        {
            streamReader.OpenSocket(multicastIP, port);

            pesAnalyser.AddCallback(*omxVideoDecode);
            
            bcm_host_init();
            omxVideoDecode->Init();

#ifdef ENABLE_OSD
            omxVideoDecode->SetPictureAnalyser(&pictureAnalyser);
            if (osdGraphics.Init())
            {
                pictureAnalyser.Init(osdGraphics);
            }
            else
            {
                std::cout << "Failed to initialise OSD graphics" << std::endl;
            }
#endif // ENABLE_OSD

            while (streamReader.Run()) {};

            std::cout << "Finished parsing transport stream (" << std::dec << tsAnalyser.getTsPacketCount() << " packets)" << std::endl;

            delete omxVideoDecode;
            tsAnalyser.DisplayStats();

            streamReader.CloseFile();
            streamReader.CloseSocket();
        }
        catch (StreamReaderException &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }

    return(0);
}
