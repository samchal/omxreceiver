#ifndef OMXVIDEODECODE_H
#define OMXVIDEODECODE_H

#include "Types.h"
#include "PesAnalyser.h"
#include "PictureAnalyser.h"

#include <stdexcept>

extern "C"
{
    #include "bcm_host.h"
    #include "ilclient.h"
};

class OmxVideoDecodeException : public std::runtime_error
{
public:
    OmxVideoDecodeException(const std::string& msg) : std::runtime_error(msg) {};
};

class OmxVideoDecode : public AcceptPesAnalyserCallbacks
{
public:
    OmxVideoDecode();
    ~OmxVideoDecode();

    void SetPictureAnalyser(PictureAnalyser *pictureAnalyser) { m_PictureAnalyser = pictureAnalyser; };
    
    void Init();
    void Run(const uint8_t *data, const uint32_t size, OMX_TICKS pts, bool ptsValid);
    void Finalise();

    // using AcceptPesAnalyserCallbacks::OnPesAnalyserCallback;
    virtual void OnPesAnalyserCallback(PesAnalyser& pesAnalyser, const AcceptPesAnalyserCallbacks::PesCallbackType callbackType);

private:
    // attributes
    PictureAnalyser *m_PictureAnalyser;
    bool m_Initialised;
    bool m_Running;

    int m_port_settings_changed;
    int m_first_packet;

    // OpenMX
    COMPONENT_T *m_video_decode;
    COMPONENT_T *m_video_scheduler;
    COMPONENT_T *m_video_render;
    COMPONENT_T *m_clock;
    COMPONENT_T *m_list[5];
    TUNNEL_T m_tunnel[4];
    ILCLIENT_T *m_client;
    
    // methods
    void PcrToOmxTicks(OMX_TICKS &pcr, const uint64_t pcrBase, const uint32_t pcrExtension) const;
    uint64_t OmxTicksToUint64(const OMX_TICKS &pcr) const;
};

#endif // OMXVIDEODECODE_H
