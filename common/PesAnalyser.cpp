#include <iostream>
#include <iomanip>

#include "PesAnalyser.h"

//------------------------------------------------------------------------------
//
PesAnalyser::PesAnalyser()
:
m_Pid(0),
m_PesPacketCount(0),
m_PesParseState(SEEK_START_CODE_0)
{
}

//------------------------------------------------------------------------------
//
PesAnalyser::PesAnalyser(TsAnalyser &tsAnalyser, uint16_t pid)
:
m_Pid(pid),
m_PesPacketCount(0),
m_PesParseState(SEEK_START_CODE_0)
{
    tsAnalyser.AddCallback(*this);    
}

//------------------------------------------------------------------------------
//
PesAnalyser::~PesAnalyser()
{
}

//------------------------------------------------------------------------------
//
void PesAnalyser::AddCallback(AcceptPesAnalyserCallbacks& callback)
{
    m_Callbacks.push_back(&callback);
}

//------------------------------------------------------------------------------
//
void PesAnalyser::RemoveCallback(AcceptPesAnalyserCallbacks& callback)
{
    std::vector<AcceptPesAnalyserCallbacks*>::iterator it;
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
void PesAnalyser::OnTsAnalyserCallback(TsAnalyser& tsAnalyser, void* context)
{
    if (tsAnalyser.get_pid() == m_Pid)
    {
        TsAnalyser::AdaptationFieldValues *adaptationFieldValues = tsAnalyser.ExtractAdaptationField();
        // check if this packet has a PCR
        if (adaptationFieldValues)
        {
            if (adaptationFieldValues->flags & AF_FLAGS_PCR_FLAG)
            {
                // signal PCR
                m_PcrBase = adaptationFieldValues->pcrBase;
                m_PcrExtension = adaptationFieldValues->pcrExtension;
                InvokeCallbacks(AcceptPesAnalyserCallbacks::PCR_SIGNALLED);
            }
        }
        
        const std::vector<uint8_t>& payload = tsAnalyser.ExtractPayload();
#if 1
        // fast PES parsing using payload unit start indicator
        if (tsAnalyser.get_payload_unit_start_indicator() && (m_PendingPacket.size() > 0))
        {
            if (tsAnalyser.ccErrorHasOccurred(m_Pid))
            {
                std::cout << "PesAnalyser::OnTsAnalyserCallback: CC error has occurred on PID " << m_Pid;
                std::cout << " - discarding PES packet" << std::endl;
            }
            else
            {
                if (m_PendingPacket.IsValid())
                {
                    InvokeCallbacks(AcceptPesAnalyserCallbacks::PES_PACKET_DECODED);
                }
            }
            m_PendingPacket.clear();
        }

        // TODO: invalidate PES packet on CC error

        // append payload
        m_PendingPacket.append(payload);
#if 0
        if (tsAnalyser.get_payload_unit_start_indicator())
        {
            std::cout << (int)payload[0] << " " << (int)payload[1] << " " << (int)payload[2] << std::endl;
        }
        std::cout << "payload.size()=" << payload.size() << ", m_PendingPacket.size()=" << m_PendingPacket.size() << std::endl;
#endif

#else
        ParseStream(payload);
#endif
    }
}

//------------------------------------------------------------------------------
//
void PesAnalyser::ParseStream(const std::vector<uint8_t>& data)
{
    std::vector<uint8_t>::const_iterator it;
    for (it = data.begin(); it != data.end(); ++it)
    {
        const uint8_t byte = *it;

        if (m_PesParseState == SEEK_START_CODE_0)
        {
            if (byte == 0)
            {
                // reset m_CurrentPacket only when a start code is in progress
                m_CurrentPacket.clear();

                if (m_PendingPacket.IsValid())
                {
                    // mark potential start code in pending packet
                    m_PendingPacket.MarkPotentialStartCode();
                }
                else
                {
                    // clear invalid pending packet data
                    m_PendingPacket.clear();
                }
            }
        }

        // accumulate m_PendingPacket data even when there might be a start code in it
        m_PendingPacket.push_back(byte);

        // accumulate m_CurrentPacket only when a start code is in progress or already detected
        if ((m_PesParseState != SEEK_START_CODE_0) || (byte == 0))
        {
            m_CurrentPacket.push_back(byte);
        }

        switch (m_PesParseState)
        {
        case SEEK_START_CODE_0:
            if (byte == 0x00)
            {
                m_PesParseState = SEEK_START_CODE_1;
            }
            break;

        case SEEK_START_CODE_1:
            if (byte == 0x00)
            {
                m_PesParseState = SEEK_START_CODE_2;
            }
            else
            {
                m_PesParseState = SEEK_START_CODE_0;
            }
            break;

        case SEEK_START_CODE_2:
            if (byte == 0x01)
            {
                m_PesParseState = SEEK_PES_HEADER;
            }
            else
            {
                m_PesParseState = SEEK_START_CODE_0;
            }
            break;

        case SEEK_PES_HEADER:
            if (m_CurrentPacket.size() >= PesPacket::PES_HEADER_LENGTH_BYTES)
            {
                if (m_CurrentPacket.IsExtendedPesHeader())
                {
                    m_SeekLength = PesPacket::PES_EXTENDED_HEADER_LENGTH_BYTES;
                    m_PesParseState = SEEK_PES_EXTENDED_HEADER;
                }
                else
                {
                    if (m_CurrentPacket.get_PES_packet_length() > 0)
                    {
                        m_SeekLength = m_CurrentPacket.size() + m_CurrentPacket.get_PES_packet_length();
                        m_PesParseState = SEEK_PES_DATA;
                    }
                    else
                    {
                        std::cerr << std::dec << "PesAnalyser::ParseStream: packet=" << m_PesPacketCount;
                        std::cerr << ", illegal PES stream - zero length payload but not video PES (short header)" << std::endl;
                        m_PesParseState = SEEK_START_CODE_0;
                    }
                }
            }
            break;

        case SEEK_PES_EXTENDED_HEADER:
            if (m_CurrentPacket.size() >= m_SeekLength)
            {
                if (m_CurrentPacket.IsValid())
                {
                    if (m_CurrentPacket.size() == PesPacket::PES_EXTENDED_HEADER_LENGTH_BYTES)
                    {
                        // check pending packet and invoke callbacks if required
                        CheckPendingPacket();
                        
                        // collect additional PES header data
                        m_SeekLength += m_CurrentPacket.get_PES_header_data_length();
                    }

                    if (m_CurrentPacket.size() == m_SeekLength)
                    {
                        // parse header
                        m_PesParseState = SEEK_START_CODE_0;
                    }
                }
                else
                {
                    m_PesParseState = SEEK_START_CODE_0;
#if 0
                    std::cerr << std::dec << "PesAnalyser::ParseStream: packet=" << m_PesPacketCount;
                    std::cerr << ", " << m_CurrentPacket.GetLastError() << std::endl;
#endif
                }
            }
            break;

        case SEEK_PES_DATA:
            if (m_CurrentPacket.size() >= m_SeekLength)
            {
                // not video

                // check pending packet and invoke callbacks if required
                CheckPendingPacket();

                m_PesParseState = SEEK_START_CODE_0;
            }
            break;

        default:
            break;
        }
    }
}

//------------------------------------------------------------------------------
//
void PesAnalyser::CheckPendingPacket()
{
    if (m_PendingPacket.IsValid())
    {
        InvokeCallbacks(AcceptPesAnalyserCallbacks::PES_PACKET_DECODED);
    }
    m_PendingPacket.clear();
}

//------------------------------------------------------------------------------
//
void PesAnalyser::InvokeCallbacks(const AcceptPesAnalyserCallbacks::PesCallbackType callbackType)
{
    std::vector<AcceptPesAnalyserCallbacks*>::const_iterator it;
    for (it = m_Callbacks.begin(); it != m_Callbacks.end(); ++it)
    {
        AcceptPesAnalyserCallbacks* callback = *it;
        callback->OnPesAnalyserCallback(*this, callbackType);
    }

    m_PesPacketCount++;
}