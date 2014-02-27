#ifndef OSDIMAGE_H
#define OSDIMAGE_H

#include "OsdGraphics.h"
#include "Types.h"

#include <stdexcept>

class OsdImageException : public std::runtime_error
{
public:
    OsdImageException(const std::string& msg) : std::runtime_error(msg) {};
};

class OsdImage
{
public:
    OsdImage(const OsdGraphics& osdGraphics, uint32_t xPos, uint32_t yPos, uint32_t width, uint32_t height);
    ~OsdImage();

    // methods
    void Render();
    void Clear(int val);
    void PutPixel(int x, int y, int val);
    void DrawLine(int x1, int y1, int x2, int y2, int val);
    void FillRect(int x, int y, int w, int h, int val);
    void PutChar(uint8_t c, int x, int y, int val);
    void DrawText(const char *text, int x, int y, int val);

private:
    // attributes
    const OsdGraphics           &m_OsdGraphics;

    void*                       m_Image;
    DISPMANX_UPDATE_HANDLE_T    m_Update;
    DISPMANX_RESOURCE_HANDLE_T  m_Resource;
    DISPMANX_ELEMENT_HANDLE_T   m_Element;
    uint32_t                    m_vcImagePtr;

    uint32_t                    m_Width;
    uint32_t                    m_Height;
    uint32_t                    m_Pitch;
    VC_IMAGE_TYPE_T             m_Type;

    // methods
};

#endif // OSDIMAGE_H
