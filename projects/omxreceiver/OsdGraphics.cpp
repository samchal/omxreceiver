#include <iostream>
#include <stdio.h>

#include "OsdGraphics.h"

//------------------------------------------------------------------------------
//
OsdGraphics::OsdGraphics()
{
}

//------------------------------------------------------------------------------
//
OsdGraphics::~OsdGraphics()
{
#if 0
    vars->update = vc_dispmanx_update_start( 10 );
    assert( vars->update );
    ret = vc_dispmanx_element_remove( vars->update, vars->element );
    assert( ret == 0 );
    ret = vc_dispmanx_update_submit_sync( vars->update );
    assert( ret == 0 );
    ret = vc_dispmanx_resource_delete( vars->resource );
    assert( ret == 0 );
    ret = vc_dispmanx_display_close( vars->display );
    assert( ret == 0 );
#endif
}

//------------------------------------------------------------------------------
//
bool OsdGraphics::Init()
{
    uint32_t screen = 0;
    
    printf("Open display[%i]...\n", screen );
    m_Display = vc_dispmanx_display_open( screen );

    if (vc_dispmanx_display_get_info( m_Display, &m_Info))
    {
        std::cout << "OsdGraphics::Init: vc_dispmanx_display_get_info failed" << std::endl;
        return(false);
    }
    printf( "Display is %d x %d\n", m_Info.width, m_Info.height );
}
