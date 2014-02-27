#ifndef PESPACKET_H
#define PESPACKET_H

#include <vector>

#include "Types.h"

class PesPacket
{
public:
    // types
    struct PesHeaderValues
    {
        uint64_t ptsBase;
        uint64_t dtsBase;
        uint32_t ptsExtension;
        uint32_t dtsExtension;
    };

    // constants
    static const uint32_t PES_HEADER_START_CODE_PREFIX = 0x000001;
    static const uint32_t PES_HEADER_LENGTH_BYTES = 6;
    static const uint32_t PES_EXTENDED_HEADER_LENGTH_BYTES = 9;

    // methods
    PesPacket();
    ~PesPacket();

    void push_back(uint8_t byte) { m_PacketData.push_back(byte); };
    void append(const std::vector<uint8_t>& data) { m_PacketData.insert(m_PacketData.end(), data.begin(), data.end()); };
    void clear() { m_PacketData.clear(); m_StartCodePos = 0; };
    size_t size() const { return(m_PacketData.size()); };

    bool IsValid();
    bool IsExtendedPesHeader() const;
    const PesHeaderValues& ExtractPesHeader();
    const std::vector<uint8_t>& ExtractPayload();

    void MarkPotentialStartCode() { m_StartCodePos = size(); };
    const char *GetLastError() const { return(m_LastErrorString); };

    // PES header
    const uint32_t get_packet_start_code_prefix() const { return (((uint32_t)m_PacketData[0] << 16) | ((uint32_t)m_PacketData[1] << 8) | (uint32_t)m_PacketData[2]); };
    const uint8_t get_stream_id() const { return m_PacketData[3]; };
    const uint16_t get_PES_packet_length() const { return (((uint16_t)m_PacketData[4] << 8) | (uint16_t)m_PacketData[5]); }

    // PES extended header
    const uint8_t get_one_zero() const { return((m_PacketData[6] >> 6) & 0x3); };
    const uint8_t get_PES_scrambling_control() const { return((m_PacketData[6] >> 4) & 0x3); };
    const bool get_PES_priority() const { return((m_PacketData[6] >> 3) & 0x1); };
    const bool get_data_alignment_indicator() const { return((m_PacketData[6] >> 2) & 0x1); };
    const bool get_copyright() const { return((m_PacketData[6] >> 1) & 0x1); };
    const bool get_original_or_copy() const { return((m_PacketData[6]) & 0x1); };
    const uint8_t get_PTS_DTS_flags() const { return((m_PacketData[7] >> 6) & 0x3); };
    const bool get_ESCR_flag() const { return((m_PacketData[7] >> 5) & 0x1); };
    const bool get_ES_rate_flag() const { return((m_PacketData[7] >> 4) & 0x1); };
    const bool get_DSM_trick_mode_flag() const { return((m_PacketData[7] >> 3) & 0x1); };
    const bool get_additional_copy_info_flag() const { return((m_PacketData[7] >> 2) & 0x1); };
    const bool get_PES_CRC_flag() const { return((m_PacketData[7] >> 1) & 0x1); };
    const bool get_PES_extension_flag() const { return((m_PacketData[7]) & 0x1); };
    const uint8_t get_PES_header_data_length() const { return(m_PacketData[8]); };

protected:
    // types

    // attributes
    PesHeaderValues m_PesHeaderValues;
    std::vector<uint8_t> m_PacketData;
    std::vector<uint8_t> m_Payload;
    uint32_t m_StartCodePos;
    char* m_LastErrorString;

    // methods

private:
    /// Disable copy constructor.
    PesPacket (const PesPacket &rhs);
    /// Disable assignment operator.
    PesPacket& operator= (const PesPacket& rhs);
};

#endif // PESPACKET_H
