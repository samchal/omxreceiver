#ifndef STREAMREADER_H
#define STREAMREADER_H

#include <vector>
#include <queue>
#include <string>
#include <stdexcept>

#include "Types.h"

class StreamReaderException : public std::runtime_error
{
public:
    StreamReaderException(const std::string& msg) : std::runtime_error(msg) {};
};

class AcceptStreamReaderCallbacks
{
public:
    virtual void OnStreamReaderCallback(class StreamReader& streamReader, void* context) = 0;
};

class StreamReader
{
public:
    // types
    struct DataFragment
    {
        uint8_t *data;
        uint32_t size;
    };

    // methods
    StreamReader();
    ~StreamReader();
    
    // add callback function when data has been received
    void AddCallback(AcceptStreamReaderCallbacks& callback);
    // remove callback function
    void RemoveCallback(AcceptStreamReaderCallbacks& callback);

    void OpenFile(const char *fileName);
    void CloseFile(void);
    void OpenSocket(const char *multicastAddr, uint16_t port);
    void CloseSocket(void);
    bool Run();
    
    bool GetDataFragment(DataFragment &dataFragment);
    const uint8_t* getDataPtr() { return m_DataPtr; };
    const uint32_t getDataSize() { return m_DataSize; };
    
    void SetUseRtp(const bool useRtp) { m_UseRtp = useRtp; };

protected:
    // constants
    static const uint32_t MAX_UDP_PDU = 65536;
    static const uint32_t TS_PACKET_SIZE_BYTES = 188;
    static const uint32_t DATA_BUFFER_SIZE = 16 * MAX_UDP_PDU;

    // attributes
    std::queue<DataFragment> m_DataFragments;
    std::vector<AcceptStreamReaderCallbacks*> m_Callbacks;
    uint8_t *m_DataBuffer;

    // methods
    void* OnThreadRun();
    bool ProcessData();
    bool ProcessRTP(uint8_t *pPacket, int32_t nSize, int32_t &nOffset, int32_t &nPayloadSize);
    void InvokeCallbacks();
    void EnumerateNetworkInterfaces();
    
private:    
    // attributes
    bool m_UseRtp;
    int m_FileHandle;
    int m_SocketHandle;
    std::vector<std::string> m_InterfaceAddresses;

    uint8_t* m_DataPtr;
    uint32_t m_DataSize;
    uint32_t m_DataOffset;
};

#endif // STREAMREADER_H
