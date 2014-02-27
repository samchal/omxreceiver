#ifndef PICTUREANALYSER_H
#define PICTUREANALYSER_H

#include <deque>

#include "Types.h"
#include "PesPacket.h"

#include "OsdGraphics.h"
#include "OsdImage.h"

class PictureAnalyser
{
public:
    PictureAnalyser();
    ~PictureAnalyser();

    // methods
    void Init(OsdGraphics &osdGraphics);
    void NewPicture(const uint8_t *data, const uint32_t size, PesPacket& pesPacket);

private:
    // constants
    static const uint32_t NUM_PICTURES_SIZES = 32;
    static const uint32_t PICTURE_SIZE_BAR_OFFSET_X = 100;
    static const uint32_t PICTURE_SIZE_BAR_OFFSET_Y = 20;
    static const uint32_t PICTURE_SIZE_BAR_WIDTH = 16;
    static const uint32_t PICTURE_SIZE_BAR_HEIGHT = 160;

    // attributes
    OsdImage *m_OsdImage;
    OsdImage *m_OsdImageTiming;
    std::deque<uint32_t> m_PictureSizes;

    // methods
};

#endif // PICTUREANALYSER_H
