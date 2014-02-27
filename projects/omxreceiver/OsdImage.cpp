#include <iostream>
#include <stdio.h>

#include "OsdImage.h"
extern "C"
{
#include "font.h"
};

#ifndef ALIGN_UP
#define ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))
#endif

#define SIGN(x) ((x > 0)? 1 : ((x < 0)? -1: 0)) 

//------------------------------------------------------------------------------
//
OsdImage::OsdImage(const OsdGraphics& osdGraphics, uint32_t xPos, uint32_t yPos, uint32_t width, uint32_t height)
:
m_OsdGraphics(osdGraphics),
m_Width(width),
m_Height(height),
m_Pitch(ALIGN_UP(m_Width*2, 32)),
m_Type(VC_IMAGE_RGB565)
{
    VC_RECT_T src_rect;
    VC_RECT_T dst_rect;
    // int aligned_height = ALIGN_UP(height, 16);
    VC_DISPMANX_ALPHA_T alpha = { static_cast<DISPMANX_FLAGS_ALPHA_T>(DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS), 
        120, /*alpha 0->255*/
        0 };

    m_Image = calloc( 1, m_Pitch * m_Height );
    if (!m_Image)
    {
        throw OsdImageException("OsdGraphics::Init: failed to allocate memory for image");
    }

    Clear(0xFFFF);
    
    m_Resource = vc_dispmanx_resource_create(m_Type, m_Width, m_Height, &m_vcImagePtr);
    if (!m_Resource)
    {
        throw OsdImageException("OsdGraphics::Init: could not create resource");
    }

    vc_dispmanx_rect_set( &dst_rect, 0, 0, m_Width, m_Height);
    if (vc_dispmanx_resource_write_data(m_Resource, m_Type, m_Pitch, m_Image, &dst_rect))
    {
        throw OsdImageException("OsdGraphics::Init: vc_dispmanx_resource_write_data failed");
    }

    m_Update = vc_dispmanx_update_start( 10 );
    if (!m_Update)
    {
        throw OsdImageException("OsdGraphics::Init: could not update");
    }

    vc_dispmanx_rect_set( &src_rect, 0, 0, m_Width << 16, m_Height << 16 );

#if 0
    vc_dispmanx_rect_set( &dst_rect, ( m_Info.width - m_Width ) / 2,
                                     ( m_Info.height - m_Height ) / 2,
                                     m_Width,
                                     m_Height );
#else
    vc_dispmanx_rect_set(&dst_rect, xPos, yPos, m_Width, m_Height);
#endif

    m_Element = vc_dispmanx_element_add(m_Update,
                                        m_OsdGraphics.GetDispManxDisplayHandle(),
                                        2000,               // layer
                                        &dst_rect,
                                        m_Resource,
                                        &src_rect,
                                        DISPMANX_PROTECTION_NONE,
                                        &alpha,
                                        NULL,             // clamp
                                        static_cast<DISPMANX_TRANSFORM_T>(VC_IMAGE_ROT0) );

    if (vc_dispmanx_update_submit_sync(m_Update))
    {
        throw OsdImageException("OsdGraphics::Init: vc_dispmanx_update_submit_sync failed");
    }
}

//------------------------------------------------------------------------------
//
OsdImage::~OsdImage()
{
}

//------------------------------------------------------------------------------
//
void OsdImage::Render()
{
    VC_RECT_T dst_rect;

    vc_dispmanx_rect_set( &dst_rect, 0, 0, m_Width, m_Height);
    if (vc_dispmanx_resource_write_data(m_Resource, m_Type, m_Pitch, m_Image, &dst_rect))
    {
        std::cout << "OsdGraphics::Render: vc_dispmanx_resource_write_data failed" << std::endl;
    }
#if 0
    else
    {
        vc_dispmanx_update_submit_sync(m_Update);
    }
#endif
}

//------------------------------------------------------------------------------
//
void OsdImage::Clear(int val)
{
    FillRect(0, 0, m_Width, m_Height, val);
}

//------------------------------------------------------------------------------
//
void OsdImage::PutPixel(int x, int y, int val)
{
    uint16_t *line = static_cast<uint16_t *>(m_Image) + y * (m_Pitch >> 1) + x;
    *line = val;
}

//------------------------------------------------------------------------------
//
void OsdImage::DrawLine(int x1, int y1, int x2, int y2, int val)
{
    int i;
    int x = x1;
    int y = y1;
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int s1 = SIGN(x2 - x1);
    int s2 = SIGN(y2 - y1);
    bool swap = false;
    if (dy > dx)
    {
        int temp = dx;
        dx = dy;
        dy = temp;
        swap = true;
    }
    int D = 2*dy - dx;
    for (i = 0; i < dx; i++)
    {
        PutPixel(x, y, val);
        while (D >= 0) 
        {
            D = D - 2*dx;
            if (swap)
                x += s1;
            else
                y += s2;
        } 
        D = D + 2*dy;
        if (swap)
            y += s2;
        else
            x += s1; 
    }
}

//------------------------------------------------------------------------------
//
void OsdImage::FillRect(int x, int y, int w, int h, int val)
{
    int row;
    int col;

    uint16_t *line = static_cast<uint16_t *>(m_Image) + y * (m_Pitch >> 1) + x;

    for (row = 0; row < h; row++)
    {
        for (col = 0; col < w; col++)
        {
            line[col] = val;
        }
        line += (m_Pitch >> 1);
    }
}

//------------------------------------------------------------------------------
//
void OsdImage::PutChar(uint8_t c, int x, int y, int val)
{
    int cx, cy;
    int mask[8] = {128,64,32,16,8,4,2,1};
    uint8_t index;
    if (c < 32)
    {
        index = 0;
    }
    else
    {
        index = c-31;
        if (c >= 160)
        {
            index = c-32;
        }
    }
    const unsigned char *glyph=font_bitmap+(int)index*16;

    for(cy=0;cy<16;cy++)
    {
        for(cx=0;cx<8;cx++)
        {
            if(glyph[cy]&mask[cx])
            {
                PutPixel(x+cx, y+cy-12, val);
            }
        }
    }
}

//------------------------------------------------------------------------------
//
void OsdImage::DrawText(const char *text, int x, int y, int val)
{
    int cx = 0;
    while (*text != 0)
    {
        PutChar(static_cast<uint8_t>(*text), x+cx, y, val);
        cx += 8;
        text++;
    }
}
