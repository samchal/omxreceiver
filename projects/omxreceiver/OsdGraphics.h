#ifndef OSDGRAPHICS_H
#define OSDGRAPHICS_H

#include "Types.h"

extern "C"
{
    #include "bcm_host.h"
};

class OsdGraphics
{
public:
    OsdGraphics();
    ~OsdGraphics();

    // methods
    bool Init();
    
    DISPMANX_DISPLAY_HANDLE_T GetDispManxDisplayHandle() const { return m_Display; };

private:
    // attributes
    DISPMANX_DISPLAY_HANDLE_T   m_Display;
    DISPMANX_MODEINFO_T         m_Info;

    // methods
};

#endif // OSDGRAPHICS_H
