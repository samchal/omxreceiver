#include <iostream>
#include <iomanip>
#include <sstream>

#include "PesAnalyser.h"
#include "OmxVideoDecode.h"

#define OMX_INIT_STRUCTURE(a) \
  memset(&(a), 0, sizeof(a)); \
  (a).nSize = sizeof(a); \
  (a).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
  (a).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
  (a).nVersion.s.nRevision = OMX_VERSION_REVISION; \
  (a).nVersion.s.nStep = OMX_VERSION_STEP

//------------------------------------------------------------------------------
//
OmxVideoDecode::OmxVideoDecode()
:
m_PictureAnalyser(NULL),
m_Initialised(false),
m_Running(false)
{
}

//------------------------------------------------------------------------------
//
OmxVideoDecode::~OmxVideoDecode()
{
    Finalise();
}

//------------------------------------------------------------------------------
//
void OmxVideoDecode::Init()
{
    m_video_decode = NULL;
    m_video_scheduler = NULL;
    m_video_render = NULL;
    m_clock = NULL;
    
    memset(m_list, 0, sizeof(m_list));
    memset(m_tunnel, 0, sizeof(m_tunnel));

    // bcm_host_init();
    
    if((m_client = ilclient_init()) == NULL)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: call to ilclient_init() failed");
    }

    if (OMX_Init() != OMX_ErrorNone)
    {
        ilclient_destroy(m_client);
        throw OmxVideoDecodeException("OmxVideoDecode::Init: call to OMX_Init() failed");
    }

    // create video_decode
    if (ilclient_create_component(m_client, &m_video_decode, "video_decode",
        static_cast<ILCLIENT_CREATE_FLAGS_T>(ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS)) != 0)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: failed to create video_decode component");
    }
    m_list[0] = m_video_decode;

    // create video_render
    if (ilclient_create_component(m_client, &m_video_render, "video_render", ILCLIENT_DISABLE_ALL_PORTS) != 0)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: failed to create video_render component");
    }
    m_list[1] = m_video_render;

    // create clock
    if (ilclient_create_component(m_client, &m_clock, "clock", ILCLIENT_DISABLE_ALL_PORTS) != 0)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: failed to create clock component");
    }
    m_list[2] = m_clock;

#if 0
    // use automatic clock start time
    OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;
    OMX_INIT_STRUCTURE(cstate);
    cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
    cstate.nWaitMask = 1;
    if (m_clock != NULL && OMX_SetParameter(ILC_GET_HANDLE(m_clock), OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: OMX_SetParameter (OMX_IndexConfigTimeClockState) failed");
    }
#endif

    // create video_scheduler
    if (ilclient_create_component(m_client, &m_video_scheduler, "video_scheduler", ILCLIENT_DISABLE_ALL_PORTS) != 0)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: failed to create video_scheduler component");
    }
    m_list[3] = m_video_scheduler;

    set_tunnel(m_tunnel, m_video_decode, 131, m_video_scheduler, 10);
    set_tunnel(m_tunnel+1, m_video_scheduler, 11, m_video_render, 90);
#if 1
    set_tunnel(m_tunnel+2, m_clock, 80, m_video_scheduler, 12);

    // setup clock tunnel first
    if (ilclient_setup_tunnel(m_tunnel+2, 0, 0) != 0)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: failed to set up clock tunnel");
    }
    
    ilclient_change_component_state(m_clock, OMX_StateExecuting);
#endif

    ilclient_change_component_state(m_video_decode, OMX_StateIdle);

    OMX_VIDEO_PARAM_PORTFORMATTYPE format;
    OMX_INIT_STRUCTURE(format);
    format.nPortIndex = 130;
    format.eCompressionFormat = OMX_VIDEO_CodingAVC;
    // format.xFramerate = 25 * (1<<16);
    
    if (OMX_SetParameter(ILC_GET_HANDLE(m_video_decode), OMX_IndexParamVideoPortFormat, &format) != OMX_ErrorNone)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: OMX_SetParameter (OMX_IndexParamVideoPortFormat) failed");
    }
    
    OMX_PARAM_PORTDEFINITIONTYPE portParam;
    OMX_INIT_STRUCTURE(portParam);
    portParam.nPortIndex = 130;
    if (OMX_GetParameter(ILC_GET_HANDLE(m_video_decode), OMX_IndexParamPortDefinition, &portParam) != OMX_ErrorNone)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: OMX_GetParameter (OMX_IndexParamPortDefinition) failed");
    }

#if 0
    portParam.nPortIndex = 130;
    portParam.nBufferCountActual = 80;
    portParam.format.video.nFrameWidth = 1920;
    portParam.format.video.nFrameHeight = 1080;
    if (OMX_SetParameter(ILC_GET_HANDLE(m_video_decode), OMX_IndexParamPortDefinition, &portParam) != OMX_ErrorNone)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: OMX_SetParameter (OMX_IndexParamPortDefinition) failed");
    }
#endif

    OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE concanParam;
    OMX_INIT_STRUCTURE(concanParam);
    concanParam.bStartWithValidFrame = OMX_FALSE;
    if (OMX_SetParameter(ILC_GET_HANDLE(m_video_decode), OMX_IndexParamBrcmVideoDecodeErrorConcealment, &concanParam) != OMX_ErrorNone)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: OMX_SetParameter (OMX_IndexParamBrcmVideoDecodeErrorConcealment) failed");
    }

    // broadcom omx entension:
    // When enabled, the timestamp fifo mode will change the way incoming timestamps are associated with output images.
    // In this mode the incoming timestamps get used without re-ordering on output images.
#if 0 // if (pts_invalid)
    OMX_CONFIG_BOOLEANTYPE timeStampMode;
    OMX_INIT_STRUCTURE(timeStampMode);
    timeStampMode.bEnabled = OMX_TRUE;
    if (OMX_SetParameter(ILC_GET_HANDLE(m_video_decode), OMX_IndexParamBrcmVideoTimestampFifo, &timeStampMode) != OMX_ErrorNone)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: OMX_SetParameter (OMX_IndexParamBrcmVideoTimestampFifo) failed");
    }
#endif

    OMX_PARAM_DATAUNITTYPE dataUnitType;
    OMX_INIT_STRUCTURE(dataUnitType);
    dataUnitType.nPortIndex = 130;
    dataUnitType.eUnitType = OMX_DataUnitCodedPicture;
    // dataUnitType.eUnitType = OMX_DataUnitArbitraryStreamSection;
    dataUnitType.eEncapsulationType = OMX_DataEncapsulationElementaryStream;

    if (OMX_SetParameter(ILC_GET_HANDLE(m_video_decode), OMX_IndexParamBrcmDataUnit, &portParam) != OMX_ErrorNone)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: OMX_SetParameter (OMX_IndexParamBrcmDataUnit) failed");
    }

    if (ilclient_enable_port_buffers(m_video_decode, 130, NULL, NULL, NULL) != OMX_ErrorNone)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Init: failed to enable video decode component port buffers");
    }

    m_Running = true;
    m_port_settings_changed = 0;
    m_first_packet = 1;

    ilclient_change_component_state(m_video_decode, OMX_StateExecuting);

    m_Initialised = true;
}

//------------------------------------------------------------------------------
//
void OmxVideoDecode::Run(const uint8_t *data, const uint32_t size, OMX_TICKS pts, bool ptsValid)
{
    OMX_BUFFERHEADERTYPE *buf;
    uint32_t dataRemaining = size;
    int data_len = 0;

    if (!m_Running)
    {
        throw OmxVideoDecodeException("OmxVideoDecode::Run: video decoder not running");
    }
#if 0
    for (int i = 0; i < 16; i++)
    {
        std::cout << std::hex << std::setw(2) << (int)data[i] << " ";
    }
    std::cout << std::dec << std::endl;
#endif

    const uint8_t *srcData = data;
    while (dataRemaining > 0)
    {
        buf = ilclient_get_input_buffer(m_video_decode, 130, 1);
        if (buf == NULL)
        {
            throw OmxVideoDecodeException("OmxVideoDecode::Run: could not get input buffer");
        }

        if (dataRemaining >= buf->nAllocLen)
        {
            data_len = buf->nAllocLen;
        }
        else
        {
            data_len = dataRemaining;
        }
        dataRemaining -= data_len;
#if 0
        std::cout << "OmxVideoDecode::Run: data_len=" << std::dec << data_len << ", dataRemaining=" << dataRemaining;
        std::cout << ", buf->nAllocLen=" << buf->nAllocLen << ", buf=0x" << std::hex << (int)buf << std::endl;
#endif
        // feed data and wait until we get port settings changed
        memcpy(buf->pBuffer, srcData, data_len);
        srcData += data_len;

        if (m_port_settings_changed == 0 &&
            ((data_len > 0 && ilclient_remove_event(m_video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1) == 0) ||
            (data_len == 0 && ilclient_wait_for_event(m_video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1,
                                                   ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 10000) == 0)))
        {
            m_port_settings_changed = 1;
            std::cout << "Port settings changed" << std::endl;

            if (ilclient_setup_tunnel(m_tunnel, 0, 0) != 0)
            {
                throw OmxVideoDecodeException("OmxVideoDecode::Run: failed to set up tunnel");
            }

            ilclient_change_component_state(m_video_scheduler, OMX_StateExecuting);

            // now setup tunnel to video_render
            if (ilclient_setup_tunnel(m_tunnel+1, 0, 1000) != 0)
            {
                throw OmxVideoDecodeException("OmxVideoDecode::Run: failed to set up tunnel");
            }

            ilclient_change_component_state(m_video_render, OMX_StateExecuting);
        }

        buf->nOffset = 0;
        buf->nFilledLen = data_len;
        data_len = 0;

        if (ptsValid)
        {
            buf->nFlags = OMX_BUFFERFLAG_STARTTIME;
            buf->nTimeStamp = pts;
#if 0
            // set clock start time based on first PTS value
            if (m_first_packet)
            {
                OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;
                OMX_INIT_STRUCTURE(cstate);
                cstate.eState = OMX_TIME_ClockStateRunning;
                cstate.nStartTime = pts;
                // cstate.nStartTime.nLowPart -= 1000000;
                if (m_clock != NULL && OMX_SetParameter(ILC_GET_HANDLE(m_clock), OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone)
                {
                    throw OmxVideoDecodeException("OmxVideoDecode::Run: failed to set clock running");
                }

                std::cout << "Clock running - pts.nHighPart=" << pts.nHighPart << ", pts.nLowPart=" << pts.nLowPart << std::endl;
                m_first_packet = 0;
            }
#endif
        }
        else
        {
            buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;
            buf->nTimeStamp.nLowPart = 0;
            buf->nTimeStamp.nHighPart = 0;
        }

        if (dataRemaining == 0)
        {
            // std::cout << "Set end of frame" << std::endl;
            buf->nFlags |= OMX_BUFFERFLAG_ENDOFFRAME;
        }

        if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(m_video_decode), buf) != OMX_ErrorNone)
        {
            throw OmxVideoDecodeException("OmxVideoDecode::Run: failed to empty buffer");
        }
    }
}

//------------------------------------------------------------------------------
//
void OmxVideoDecode::Finalise()
{
    if (m_Running)
    {
        OMX_BUFFERHEADERTYPE *buf = ilclient_get_input_buffer(m_video_decode, 130, 1);
        if (buf != NULL)
        {
            buf->nFilledLen = 0;
            buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;

            if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(m_video_decode), buf) != OMX_ErrorNone)
            {
                // handle error here
            }

            // wait for EOS from render
            ilclient_wait_for_event(m_video_render, OMX_EventBufferFlag, 90, 0, OMX_BUFFERFLAG_EOS, 0,
                ILCLIENT_BUFFER_FLAG_EOS, 10000);

            // need to flush the renderer to allow video_decode to disable its input port
            ilclient_flush_tunnels(m_tunnel, 0);

            ilclient_disable_port_buffers(m_video_decode, 130, NULL, NULL, NULL);
            
            m_Running = false;
        }
    }
    
    if (m_Initialised)
    {
        ilclient_disable_tunnel(m_tunnel);
        ilclient_disable_tunnel(m_tunnel+1);
        ilclient_disable_tunnel(m_tunnel+2);
        ilclient_teardown_tunnels(m_tunnel);

        ilclient_state_transition(m_list, OMX_StateIdle);
        ilclient_state_transition(m_list, OMX_StateLoaded);

        ilclient_cleanup_components(m_list);

        OMX_Deinit();

        ilclient_destroy(m_client);
        
        m_Initialised = false;
    }
}

//------------------------------------------------------------------------------
//
void OmxVideoDecode::OnPesAnalyserCallback(PesAnalyser& pesAnalyser, const AcceptPesAnalyserCallbacks::PesCallbackType callbackType)
{
    // std::cout << "OmxVideoDecode::OnPesAnalyserCallback" << std::endl;

    if (callbackType == AcceptPesAnalyserCallbacks::PES_PACKET_DECODED)
    {
        bool ptsValid = false;
        OMX_TICKS pts;
        PesPacket& pesPacket(pesAnalyser.getPacket());
#if 1
        pts.nLowPart = pts.nHighPart = 0;
        if (pesPacket.IsExtendedPesHeader())
        {
            PesPacket::PesHeaderValues pesHeaderValues(pesPacket.ExtractPesHeader());
            if (pesPacket.get_PTS_DTS_flags() & 0x2)
            {
                PcrToOmxTicks(pts, pesHeaderValues.ptsBase, 0);
                // std::cout << "pts=" << std::dec << OmxTicksToUint64(pts) << std::endl;
                ptsValid = true;
            }
        }
#endif
        const std::vector<uint8_t> &payload = pesPacket.ExtractPayload();
        Run(&payload[0], payload.size(), pts, ptsValid);
        
        if (m_PictureAnalyser)
        {
            m_PictureAnalyser->NewPicture(&payload[0], payload.size(), pesPacket);
        }
    }
    else if (callbackType == AcceptPesAnalyserCallbacks::PCR_SIGNALLED)
    {
        OMX_TICKS pcr;
        PcrToOmxTicks(pcr, pesAnalyser.getPcrBase(), pesAnalyser.getPcrExtension());
        // std::cout << "pcrBase=0x" << std::hex << pesAnalyser.getPcrBase() << ", pcrExtension=0x" << pesAnalyser.getPcrExtension() << std::endl;
        // std::cout << "pcr=" << std::dec << OmxTicksToUint64(pcr) << std::endl;
#if 1
        // set clock start time based on first PCR timestamp received
        if (m_first_packet)
        {
            OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;
            OMX_INIT_STRUCTURE(cstate);
            cstate.eState = OMX_TIME_ClockStateRunning;
            cstate.nStartTime = pcr;
            if (m_clock != NULL && OMX_SetParameter(ILC_GET_HANDLE(m_clock), OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone)
            {
                throw OmxVideoDecodeException("OmxVideoDecode::Run: failed to set clock running");
            }

            std::cout << "Clock running - pcr=" << std::dec << OmxTicksToUint64(pcr) << std::endl;
            m_first_packet = 0;
        }
#endif
    }
}

//------------------------------------------------------------------------------
//
void OmxVideoDecode::PcrToOmxTicks(OMX_TICKS &pcr, const uint64_t pcrBase, const uint32_t pcrExtension) const
//
/// @brief PcrToOmxTimestamp Convert PCR base/extension to OMX value (us)
///
/// OMX timestamp is a 64-bit value in microseconds.
/// TODO: modify this function to handle PCR wraparound
//
//------------------------------------------------------------------------------
{
    uint64_t temp = (pcrBase * 300 + pcrExtension) / 27;
    
    pcr.nLowPart = static_cast<uint32_t>(temp);
    pcr.nHighPart = static_cast<uint32_t>(temp >> 32);
}

//------------------------------------------------------------------------------
//
uint64_t OmxVideoDecode::OmxTicksToUint64(const OMX_TICKS &pcr) const
{
    uint64_t temp = (static_cast<uint64_t>(pcr.nHighPart) << 32) + static_cast<uint64_t>(pcr.nLowPart);
    return(temp);
}