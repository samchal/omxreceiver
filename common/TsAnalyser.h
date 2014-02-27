#ifndef TSANALYSER_H
#define TSANALYSER_H

#include <map>
#include <vector>

#include "Types.h"
#include "StreamReader.h"

// adaptation_field_control bits
#define AFC_ADAPTATION_FIELD                        (1<<1)
#define AFC_PAYLOAD                                 (1<<0)

// adaptation field flags
#define AF_FLAGS_DISCONTINUITY_INDICATOR            (1<<7)
#define AF_FLAGS_RANDOM_ACCESS_INDICATOR            (1<<6)
#define AF_FLAGS_ELEM_STREAM_PRIORITY_INDICATOR     (1<<5)
#define AF_FLAGS_PCR_FLAG                           (1<<4)
#define AF_FLAGS_OPCR_FLAG                          (1<<3)
#define AF_FLAGS_SPLICING_POINT_FLAG                (1<<2)
#define AF_FLAGS_TRANSPORT_PRIVATE_DATA_FLAG        (1<<1)
#define AF_FLAGS_ADAPTATION_FIELD_EXT_FLAG          (1<<0)

class AcceptTsAnalyserCallbacks
{
public:
    virtual void OnTsAnalyserCallback(class TsAnalyser& tsAnalyser, void* context) = 0;
};

class TsAnalyser : public AcceptStreamReaderCallbacks
{
public:
    struct AdaptationFieldValues
    {
#if 0
        union afFlags_t
        {
            uint8_t byte;
            struct afBits_t
            {
                uint8_t discont_indicator               : 1;
                uint8_t rand_access_indicator           : 1;
                uint8_t elem_stream_priority_indicator  : 1;
                uint8_t PCR_flag                        : 1;
                uint8_t OPCR_flag                       : 1;
                uint8_t splicing_point_flag             : 1;
                uint8_t transport_private_data_flag     : 1;
                uint8_t get_adaptation_field_ext_flag   : 1;
            } bits;
        };
        afFlags_t flags;
#endif
        uint8_t flags;
        uint64_t pcrBase;
        uint64_t opcrBase;
        uint32_t pcrExtension;
        uint32_t opcrExtension;
        int8_t spliceCountdown;
        std::vector<uint8_t> privateData;
    };
    struct PidInfo
    {
        uint32_t packetCount;
        uint32_t ccErrorCount;
        bool ccErrorHasOccurred;
        uint8_t lastContinuityCount;
    };

    TsAnalyser();
    TsAnalyser(StreamReader &streamReader);
    ~TsAnalyser();

    // add callback function when TS packet has been parsed
    void AddCallback(AcceptTsAnalyserCallbacks& callback);
    // remove callback function
    void RemoveCallback(AcceptTsAnalyserCallbacks& callback);

    void OnStreamReaderCallback(StreamReader& streamReader, void* context);
    void ParseStream(const uint8_t *data, const uint32_t size);
    AdaptationFieldValues* ExtractAdaptationField();
    const std::vector<uint8_t>& ExtractPayload();
    const uint8_t *GetPayload(uint32_t &size);
    bool ccErrorHasOccurred(uint16_t pid);
    void DisplayStats() const;

    uint64_t getTsPacketCount() { return m_TsPacketCount; };

    // TS packet header
    const uint8_t get_sync_byte() const { return m_PacketBuffer[0]; };
    const bool get_transport_error_indicator() const { return ((m_PacketBuffer[1] >> 7) & 0x1); };
    const bool get_payload_unit_start_indicator() const { return ((m_PacketBuffer[1] >> 6) & 0x1); };
    const bool get_transport_priority() const { return ((m_PacketBuffer[1] >> 5) & 0x1); };
    const uint16_t get_pid() const { return ((((uint16_t)m_PacketBuffer[1] & 0x1F) << 8) | (uint16_t)m_PacketBuffer[2]); };
    const uint8_t get_transport_scrambling_control() const { return ((m_PacketBuffer[3] >> 6) & 0x3); };
    const uint8_t get_adaptation_field_control() const { return ((m_PacketBuffer[3] >> 4) & 0x3); };
    const uint8_t get_continuity_counter() const { return ((m_PacketBuffer[3]) & 0xF); };

    // adaptation field
    const uint8_t get_adaptation_field_length() const { return m_PacketBuffer[TS_HEADER_SIZE_BYTES]; };

protected:
    // constants
    static const uint32_t TS_PACKET_SIZE_BYTES = 188;
    static const uint32_t TS_HEADER_SIZE_BYTES = 4;

    // attributes
    std::vector<AcceptTsAnalyserCallbacks*> m_Callbacks;

    // TS packet data buffers
    uint8_t m_PacketBuffer[TS_PACKET_SIZE_BYTES];
    AdaptationFieldValues m_AdaptationFieldValues;
    std::vector<uint8_t> m_Payload;

    // packet count
    uint64_t m_TsPacketCount;

    // PID information
    std::map<uint16_t, PidInfo> m_PidMap;

    // methods
    void InvokeCallbacks();

private:
    /// Disable copy constructor.
    TsAnalyser (const TsAnalyser &rhs);
    /// Disable assignment operator.
    TsAnalyser& operator= (const TsAnalyser& rhs);
};

#endif // TSANALYSER_H
