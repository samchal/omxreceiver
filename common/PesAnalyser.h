#ifndef PESANALYSER_H
#define PESANALYSER_H

#include <vector>

#include "PesPacket.h"
#include "TsAnalyser.h"
#include "Types.h"

class AcceptPesAnalyserCallbacks
{
public:
    // types
    enum PesCallbackType
    {
        PES_PACKET_DECODED = 0,
        PCR_SIGNALLED
    };
    
    // methods
    virtual void OnPesAnalyserCallback(class PesAnalyser& pesAnalyser, const PesCallbackType callbackType) = 0;
};

class PesAnalyser : public AcceptTsAnalyserCallbacks
{
public:
    // methods
    PesAnalyser();
    PesAnalyser(TsAnalyser &tsAnalyser, uint16_t pid);
    ~PesAnalyser();

    // add callback function when PES packet has been parsed
    void AddCallback(AcceptPesAnalyserCallbacks& callback);
    // remove callback function
    void RemoveCallback(AcceptPesAnalyserCallbacks& callback);

    void OnTsAnalyserCallback(TsAnalyser& tsAnalyser, void* context);
    void ParseStream(const std::vector<uint8_t>& data);

    const uint64_t getPesPacketCount() { return m_PesPacketCount; };
    PesPacket& getPacket() { return m_PendingPacket; };
    
    const uint64_t getPcrBase() { return m_PcrBase; };
    const uint32_t getPcrExtension() { return m_PcrExtension; };

protected:
    // types
    enum PesParseState
    {
        SEEK_START_CODE_0 = 0,
        SEEK_START_CODE_1,
        SEEK_START_CODE_2,
        SEEK_PES_HEADER,
        SEEK_PES_EXTENDED_HEADER,
        SEEK_PES_DATA
    };

    // attributes
    std::vector<AcceptPesAnalyserCallbacks*> m_Callbacks;

    uint16_t m_Pid;                         // TS analyser PID
    uint64_t m_PesPacketCount;              // PES packet count
    PesParseState m_PesParseState;          // current PES parse state
    uint32_t m_SeekLength;                  // amount of data to seek in next state

    PesPacket m_PendingPacket;
    PesPacket m_CurrentPacket;
    
    // relayed values from TsAnalyser
    uint64_t m_PcrBase;
    uint32_t m_PcrExtension;

    // methods
    void CheckPendingPacket();
    void InvokeCallbacks(const AcceptPesAnalyserCallbacks::PesCallbackType callbackType);

private:
    /// Disable copy constructor.
    PesAnalyser (const PesAnalyser &rhs);
    /// Disable assignment operator.
    PesAnalyser& operator= (const PesAnalyser& rhs);
};

#endif // PESANALYSER_H
