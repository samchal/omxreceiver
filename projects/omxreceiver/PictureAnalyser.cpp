#include <iostream>
#include <sstream>

#include "PictureAnalyser.h"

PictureAnalyser::PictureAnalyser()
:
m_OsdImage(0),
m_OsdImageTiming(0)
{
    m_PictureSizes.resize(NUM_PICTURES_SIZES, 0);
}

PictureAnalyser::~PictureAnalyser()
{
    if (m_OsdImage)
    {
        delete m_OsdImage;
    }
    if (m_OsdImageTiming)
    {
        delete m_OsdImageTiming;
    }
}

void PictureAnalyser::Init(OsdGraphics &osdGraphics)
{
    m_OsdImage = new OsdImage(osdGraphics, 32, 96, 640, 200);
    m_OsdImageTiming = new OsdImage(osdGraphics, 700, 96, 640, 200);
}

void PictureAnalyser::NewPicture(const uint8_t *data, const uint32_t size, PesPacket& pesPacket)
{
    // std::cout << "New picture received (size=" << size << " bytes)" << std::endl;  
    m_PictureSizes.pop_front();
    m_PictureSizes.push_back(size * 8);     // picture size in bits
    
    uint32_t maxPictureSize = 0;
    std::deque<uint32_t>::iterator it;
    for (it = m_PictureSizes.begin(); it != m_PictureSizes.end(); ++it)
    {
        if (*it > maxPictureSize)
        {
            maxPictureSize = *it;
        }
    }
    
    // round maxPictureSize to nearest megabit
    maxPictureSize = (maxPictureSize+999999)/1000000;
    maxPictureSize *= 1000000;

    if (m_OsdImage)
    {
        m_OsdImage->Clear(0x0000);        
        m_OsdImage->DrawLine(PICTURE_SIZE_BAR_OFFSET_X-1, PICTURE_SIZE_BAR_OFFSET_Y, 
            PICTURE_SIZE_BAR_OFFSET_X-1, PICTURE_SIZE_BAR_OFFSET_Y + PICTURE_SIZE_BAR_HEIGHT, 0xFFFF);
        for (uint32_t n = 0; n <= 5; n++)
        {
            std::stringstream ss;
            uint32_t y = PICTURE_SIZE_BAR_OFFSET_Y + (n * PICTURE_SIZE_BAR_HEIGHT)/5;
            uint32_t bits = maxPictureSize * (5-n) / (1000 * 5);
            
            ss << std::dec << bits;

            m_OsdImage->DrawLine(PICTURE_SIZE_BAR_OFFSET_X-5, y, PICTURE_SIZE_BAR_OFFSET_X, y, 0xFFFF);
            m_OsdImage->DrawText(ss.str().c_str(), PICTURE_SIZE_BAR_OFFSET_X-40, y, 0xFFFF);
        }
        m_OsdImage->DrawLine(PICTURE_SIZE_BAR_OFFSET_X, PICTURE_SIZE_BAR_OFFSET_Y + PICTURE_SIZE_BAR_HEIGHT,
            PICTURE_SIZE_BAR_OFFSET_X + (NUM_PICTURES_SIZES * PICTURE_SIZE_BAR_WIDTH), PICTURE_SIZE_BAR_OFFSET_Y + PICTURE_SIZE_BAR_HEIGHT, 0xFFFF);

        uint32_t index = 0;
        for (it = m_PictureSizes.begin(); it != m_PictureSizes.end(); ++it)
        {
            uint32_t barHeight = (PICTURE_SIZE_BAR_HEIGHT * (*it)) / maxPictureSize;
            m_OsdImage->FillRect(PICTURE_SIZE_BAR_OFFSET_X + (index * PICTURE_SIZE_BAR_WIDTH), 
                PICTURE_SIZE_BAR_OFFSET_Y + PICTURE_SIZE_BAR_HEIGHT - barHeight, PICTURE_SIZE_BAR_WIDTH, barHeight, 0xF800);
            index++;
        }

        m_OsdImage->Render();
    }

    if (m_OsdImageTiming)
    {
        if (pesPacket.IsExtendedPesHeader())
        {
            PesPacket::PesHeaderValues pesHeaderValues(pesPacket.ExtractPesHeader());

            m_OsdImageTiming->Clear(0x0000);

            if (pesPacket.get_PTS_DTS_flags() & 0x2)
            {
                std::stringstream ss;
                ss << "PTS = 0x" << std::hex << pesHeaderValues.ptsBase;
                m_OsdImageTiming->DrawText(ss.str().c_str(), 16, 16, 0xFFFF);
            }
            if (pesPacket.get_PTS_DTS_flags() & 0x1)
            {
                std::stringstream ss;
                ss << "DTS = 0x" << std::hex << pesHeaderValues.dtsBase;
                m_OsdImageTiming->DrawText(ss.str().c_str(), 16, 32, 0xFFFF);
            }
        }
        
        m_OsdImageTiming->Render();
    }
}
