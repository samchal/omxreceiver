#include <string.h>

#include <iostream>
#include <iomanip>

#include "TsAnalyser.h"

static const uint8_t TS_SYNC_BYTE = 0x47;
static const uint32_t NULL_PACKET_PID = 0x1FFF;

//------------------------------------------------------------------------------
//
TsAnalyser::TsAnalyser()
:
m_TsPacketCount(0)
{
    // reserve space for payload
    m_Payload.reserve(TS_PACKET_SIZE_BYTES - TS_HEADER_SIZE_BYTES);
    // reserve space for adaptation field private data
    m_AdaptationFieldValues.privateData.reserve(TS_PACKET_SIZE_BYTES - TS_HEADER_SIZE_BYTES - 3);
}

//------------------------------------------------------------------------------
//
TsAnalyser::TsAnalyser(StreamReader &streamReader)
:
m_TsPacketCount(0)
{
    // reserve space for payload
    m_Payload.reserve(TS_PACKET_SIZE_BYTES - TS_HEADER_SIZE_BYTES);
    // reserve space for adaptation field private data
    m_AdaptationFieldValues.privateData.reserve(TS_PACKET_SIZE_BYTES - TS_HEADER_SIZE_BYTES - 3);
    
    streamReader.AddCallback(*this); 
}

//------------------------------------------------------------------------------
//
TsAnalyser::~TsAnalyser()
{
}

//------------------------------------------------------------------------------
//
void TsAnalyser::AddCallback(AcceptTsAnalyserCallbacks& callback)
{
    m_Callbacks.push_back(&callback);
}

//------------------------------------------------------------------------------
//
void TsAnalyser::RemoveCallback(AcceptTsAnalyserCallbacks& callback)
{
    std::vector<AcceptTsAnalyserCallbacks*>::iterator it;
    for (it = m_Callbacks.begin(); it != m_Callbacks.end(); ++it)
    {
        if (*it == &callback)
        {
            m_Callbacks.erase(it);
        }
    }
}

//------------------------------------------------------------------------------
//
void TsAnalyser::OnStreamReaderCallback(StreamReader& streamReader, void* context)
{
    const uint8_t *data = streamReader.getDataPtr();

    if (data)
    {
#if 0
        std::cout << "Received " << std::dec << streamReader.getDataSize() << " bytes, ";
        std::cout << "first byte=" << std::hex << (int)data[0] << std::endl;
#endif
        ParseStream(data, streamReader.getDataSize());
    }
}

//------------------------------------------------------------------------------
//
void TsAnalyser::ParseStream(const uint8_t *data, const uint32_t size)
{
    bool finished = false;
    uint32_t bytesRemaining = size;

    while (!finished)
    {
        if (bytesRemaining < TS_PACKET_SIZE_BYTES)
        {
            // reached end of input
            finished = true;
        }
        else
        {
            memcpy(static_cast<void *>(m_PacketBuffer), static_cast<const void *>(data), TS_PACKET_SIZE_BYTES);
            data += TS_PACKET_SIZE_BYTES;
            bytesRemaining -= TS_PACKET_SIZE_BYTES;
            
            if (m_PacketBuffer[0] != TS_SYNC_BYTE)
            {
                // failed to find sync byte
                // TODO: resync here?
                std::cerr << "TsAnalyser::ParseStream: packet=" << std::dec << m_TsPacketCount;
                std::cerr << ", no sync byte found" << std::endl;
                finished = true;
            }
        }
        
        if (!finished)
        {
            uint16_t pid = get_pid();
            if (m_PidMap.count(pid))
            {
                PidInfo &pidInfo = m_PidMap[pid];
                pidInfo.packetCount++;

                if (get_pid() != NULL_PACKET_PID)
                {
                    uint8_t expectedContinuityCount = pidInfo.lastContinuityCount;
                    if (get_adaptation_field_control() & AFC_PAYLOAD)
                    {
                        // continuity count should only increment on packets with data payloads
                        expectedContinuityCount = (expectedContinuityCount + 1) & 0xF;
                    }
                    if (get_continuity_counter() != expectedContinuityCount)
                    {
                        std::cerr << std::hex << "TsAnalyser::ParseStream: continuity count error on PID=0x" << pid;
                        std::cerr << " (expected CC=" << (uint32_t)expectedContinuityCount;
                        std::cerr << ", actual CC=" << (uint32_t)get_continuity_counter() << ")" << std::endl;
                        pidInfo.ccErrorCount++;
                        pidInfo.ccErrorHasOccurred = true;
                    }
                    pidInfo.lastContinuityCount = get_continuity_counter();
                }
            }
            else
            {
                m_PidMap[pid].packetCount = 1;
                m_PidMap[pid].ccErrorCount = 0;
                m_PidMap[pid].ccErrorHasOccurred = false;
                m_PidMap[pid].lastContinuityCount = get_continuity_counter();
            }

            InvokeCallbacks();
        }
    }
}

//------------------------------------------------------------------------------
//
TsAnalyser::AdaptationFieldValues* TsAnalyser::ExtractAdaptationField()
{
    AdaptationFieldValues* adaptationFieldValues = 0;
    m_AdaptationFieldValues.privateData.clear();

    if ((get_adaptation_field_control() & AFC_ADAPTATION_FIELD) &&
        (get_adaptation_field_length() > 0))
    {
        uint32_t afPosition = TS_HEADER_SIZE_BYTES + 1;
        if (get_adaptation_field_length() > (TS_PACKET_SIZE_BYTES - afPosition))
        {
            std::cerr << std::dec << "TsAnalyser::ExtractAdaptationField: packet=" << m_TsPacketCount;
            std::cerr << ", invalid adaptation_field_length=" << get_adaptation_field_length();
            std::cerr << " (remaining bytes in TS packet=" << (TS_PACKET_SIZE_BYTES - afPosition) << ")" << std::endl;
        }

        m_AdaptationFieldValues.flags = m_PacketBuffer[afPosition++];
        if (m_AdaptationFieldValues.flags & AF_FLAGS_PCR_FLAG)
        {
            m_AdaptationFieldValues.pcrBase = (uint64_t)m_PacketBuffer[afPosition++] << 25;
            m_AdaptationFieldValues.pcrBase |= (uint64_t)m_PacketBuffer[afPosition++] << 17;
            m_AdaptationFieldValues.pcrBase |= (uint64_t)m_PacketBuffer[afPosition++] << 9;
            m_AdaptationFieldValues.pcrBase |= (uint64_t)m_PacketBuffer[afPosition++] << 1;
            m_AdaptationFieldValues.pcrBase |= ((uint64_t)m_PacketBuffer[afPosition] >> 7) & 0x1;
            m_AdaptationFieldValues.pcrExtension = (uint32_t)(m_PacketBuffer[afPosition++] & 0x1) << 8;
            m_AdaptationFieldValues.pcrExtension |= (uint32_t)m_PacketBuffer[afPosition++];
        }
        if (m_AdaptationFieldValues.flags & AF_FLAGS_OPCR_FLAG)
        {
            m_AdaptationFieldValues.opcrBase = (uint64_t)m_PacketBuffer[afPosition++] << 25;
            m_AdaptationFieldValues.opcrBase |= (uint64_t)m_PacketBuffer[afPosition++] << 17;
            m_AdaptationFieldValues.opcrBase |= (uint64_t)m_PacketBuffer[afPosition++] << 9;
            m_AdaptationFieldValues.opcrBase |= (uint64_t)m_PacketBuffer[afPosition++] << 1;
            m_AdaptationFieldValues.opcrBase |= ((uint64_t)m_PacketBuffer[afPosition] >> 7) & 0x1;
            m_AdaptationFieldValues.opcrExtension = (uint32_t)(m_PacketBuffer[afPosition++] & 0x1) << 8;
            m_AdaptationFieldValues.opcrExtension |= (uint32_t)m_PacketBuffer[afPosition++];
        }
        if (m_AdaptationFieldValues.flags & AF_FLAGS_SPLICING_POINT_FLAG)
        {
            m_AdaptationFieldValues.spliceCountdown = m_PacketBuffer[afPosition++];
        }
        if (m_AdaptationFieldValues.flags & AF_FLAGS_TRANSPORT_PRIVATE_DATA_FLAG)
        {
            uint32_t transport_private_data_length = m_PacketBuffer[afPosition++];
            if (transport_private_data_length > (TS_PACKET_SIZE_BYTES - afPosition))
            {
                std::cerr << std::dec << "TsAnalyser::ExtractAdaptationField: packet=" << m_TsPacketCount;
                std::cerr << ", invalid transport_private_data_length=" << transport_private_data_length;
                std::cerr << " (remaining bytes in TS packet=" << (TS_PACKET_SIZE_BYTES - afPosition) << ")" << std::endl;
            }
            else
            {
                for (uint32_t pos = 0; pos < transport_private_data_length; pos++)
                {
                    m_AdaptationFieldValues.privateData.push_back(m_PacketBuffer[afPosition++]);
                }
            }
        }
        if (m_AdaptationFieldValues.flags & AF_FLAGS_ADAPTATION_FIELD_EXT_FLAG)
        {
            uint8_t adaptation_field_extension_length = m_PacketBuffer[afPosition++];

            // TODO: extract adaptation field extension here
            afPosition += adaptation_field_extension_length;
        }

        adaptationFieldValues = &m_AdaptationFieldValues;
    }

    return(adaptationFieldValues);
}

//------------------------------------------------------------------------------
//
const uint8_t *TsAnalyser::GetPayload(uint32_t &size)
{
    uint32_t packetPosition = TS_HEADER_SIZE_BYTES;
    uint8_t *payloadPtr = NULL;
    size = 0;
    
    if (get_adaptation_field_control() & AFC_PAYLOAD)
    {
        if (get_adaptation_field_control() & AFC_ADAPTATION_FIELD)
        {
            // skip over adaptation field
            packetPosition += (get_adaptation_field_length() + 1);
        }
        
        payloadPtr = &m_PacketBuffer[packetPosition];
        size = TS_PACKET_SIZE_BYTES - packetPosition;
    }
    
    return(payloadPtr);
}

//------------------------------------------------------------------------------
//
const std::vector<uint8_t>& TsAnalyser::ExtractPayload()
{
    uint32_t packetPosition = TS_HEADER_SIZE_BYTES;
    m_Payload.clear();

    if (get_adaptation_field_control() & AFC_PAYLOAD)
    {
        if (get_adaptation_field_control() & AFC_ADAPTATION_FIELD)
        {
            // skip over adaptation field
            packetPosition += (get_adaptation_field_length() + 1);
        }
        while (packetPosition < TS_PACKET_SIZE_BYTES)
        {
            m_Payload.push_back(m_PacketBuffer[packetPosition]);
            packetPosition++;
        }
    }

    return(m_Payload);
}

//------------------------------------------------------------------------------
//
bool TsAnalyser::ccErrorHasOccurred(uint16_t pid)
{
    bool ccError = false;
    
    if (m_PidMap.count(pid))
    {
        if (m_PidMap[pid].ccErrorHasOccurred)
        {
            ccError = true;
            m_PidMap[pid].ccErrorHasOccurred = false;
        }
    }
    
    return(ccError);
}

//------------------------------------------------------------------------------
//
void TsAnalyser::DisplayStats() const
{
    std::cout << "PID(Dec) PID(Hex) Packets    CC errors" << std::endl;
    std::cout << "======== ======== ========== ==========" << std::endl;
    std::map<uint16_t, PidInfo>::const_iterator itr;

    for (itr = m_PidMap.begin(); itr != m_PidMap.end(); itr++)
    {
        std::cout << std::dec << std::setw(8) << (*itr).first;
        std::cout << " " << std::hex << std::setw(8) << std::showbase << (*itr).first;
        std::cout << " " << std::dec << std::setw(10) << (*itr).second.packetCount;
        std::cout << " " << std::dec << std::setw(10) << (*itr).second.ccErrorCount;
        std::cout << std::endl;
    }
}

//------------------------------------------------------------------------------
//
void TsAnalyser::InvokeCallbacks()
{
    // std::cout << "TsAnalyser::InvokeCallbacks" << std::endl;
    
    std::vector<AcceptTsAnalyserCallbacks*>::const_iterator it;
    for (it = m_Callbacks.begin(); it != m_Callbacks.end(); ++it)
    {
        AcceptTsAnalyserCallbacks* callback = *it;
        callback->OnTsAnalyserCallback(*this, 0);
    }

    m_TsPacketCount++;
}
