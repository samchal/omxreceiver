#include <iostream>
#include <iomanip>

#include "PesPacket.h"

//------------------------------------------------------------------------------
//
PesPacket::PesPacket()
:
m_StartCodePos(0)
{    
}

//------------------------------------------------------------------------------
//
PesPacket::~PesPacket()
{
}

//------------------------------------------------------------------------------
//
bool PesPacket::IsValid()
{
    bool IsValid = false;

    if (size() >= PES_HEADER_LENGTH_BYTES)
    {
        if (get_packet_start_code_prefix() == PES_HEADER_START_CODE_PREFIX)
        {
            IsValid = true;
        }
    }

    if (IsValid && IsExtendedPesHeader())
    {
        if (get_one_zero() != 2)
        {
            IsValid = false;
        }

        if (!IsValid)
        {
            m_LastErrorString = "error in extended header";
        }
    }

    return(IsValid);
}
//------------------------------------------------------------------------------
//
const PesPacket::PesHeaderValues& PesPacket::ExtractPesHeader()
{
    if (IsExtendedPesHeader())
    {
        uint32_t pesHdrPos = PES_EXTENDED_HEADER_LENGTH_BYTES;

        if (get_PTS_DTS_flags() & 0x2)
        {
            m_PesHeaderValues.ptsBase = (uint64_t)(m_PacketData[pesHdrPos++] & 0x0E) << 29;
            m_PesHeaderValues.ptsBase |= (uint64_t)m_PacketData[pesHdrPos++] << 22;
            m_PesHeaderValues.ptsBase |= (uint64_t)(m_PacketData[pesHdrPos++] & 0xFE) << 14;
            m_PesHeaderValues.ptsBase |= (uint64_t)m_PacketData[pesHdrPos++] << 7;
            m_PesHeaderValues.ptsBase |= (uint64_t)(m_PacketData[pesHdrPos++] & 0xFE) >> 1;
        }
        if (get_PTS_DTS_flags() & 0x1)
        {
            m_PesHeaderValues.dtsBase = (uint64_t)(m_PacketData[pesHdrPos++] & 0x0E) << 29;
            m_PesHeaderValues.dtsBase |= (uint64_t)m_PacketData[pesHdrPos++] << 22;
            m_PesHeaderValues.dtsBase |= (uint64_t)(m_PacketData[pesHdrPos++] & 0xFE) << 14;
            m_PesHeaderValues.dtsBase |= (uint64_t)m_PacketData[pesHdrPos++] << 7;
            m_PesHeaderValues.dtsBase |= (uint64_t)(m_PacketData[pesHdrPos++] & 0xFE) >> 1;
        }
    }

    return(m_PesHeaderValues);
}

//------------------------------------------------------------------------------
//
const std::vector<uint8_t>& PesPacket::ExtractPayload()
{
    uint32_t payloadStartPos;

    m_Payload.clear();
    if (IsValid())
    {
        if (m_StartCodePos > 0)
        {
            m_PacketData.resize(m_StartCodePos);
            m_StartCodePos = 0;
        }

        if (IsExtendedPesHeader())
        {
            payloadStartPos = PES_EXTENDED_HEADER_LENGTH_BYTES + get_PES_header_data_length();
        }
        else
        {
            payloadStartPos = PES_HEADER_LENGTH_BYTES;
        }

        std::vector<uint8_t>::const_iterator it;
        m_Payload.assign(m_PacketData.begin() + payloadStartPos, m_PacketData.end());
    }

    return(m_Payload);
}

//------------------------------------------------------------------------------
//
bool PesPacket::IsExtendedPesHeader() const
{
    bool IsExtendedPesHeader = false;

    if (size() >= PES_HEADER_LENGTH_BYTES)
    {
        const uint8_t streamId = get_stream_id();

        if (streamId != 0xBC &&     // program_stream_map
            streamId != 0xBE &&     // padding_stream
            streamId != 0xBF &&     // private_stream_2
            streamId != 0xF0 &&     // ECM
            streamId != 0xF1 &&     // EMM
            streamId != 0xFF &&     // program_stream_directory
            streamId != 0xF2 &&     // DSMCC_stream
            streamId != 0xF8)       // ITU-T Rec. H222.1 type E stream
        {
            IsExtendedPesHeader = true;
        }
    }

    return(IsExtendedPesHeader);
}
