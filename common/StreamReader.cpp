#include <unistd.h>
#include <sys/fcntl.h>

// networking
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <iostream>
#include <iomanip>
#include <sstream>

#include <string.h>
#include "StreamReader.h"

#define HOSTNAMELEN             256

// Used by the RTP header parsing
// Constants
#define MIN_RTP_HEADER          12
#define EXPECTED_RTP_VERSION    0x80
#define CSRC_LEN                4   //bytes
#define SEQUENCE_OFFSET         2

// Macros
#define RTP_VERSION(x)          (x[0]&0xC0)
#define HAS_EXTENSION(x)        (x[0]&0x10)
#define HAS_PADDING(x)          (x[0]&0x20)
#define CSRC_COUNT(x)           (x[0]&0x0F)

#define MAX_UDP_PDU             65536       // Max UDP packet size is 64 Kbyte
#define KERNEL_RX_BUFFER_SIZE   256*1024

//------------------------------------------------------------------------------
//
StreamReader::StreamReader()
:
m_DataBuffer(0),
m_UseRtp(false),
m_FileHandle(-1),
m_SocketHandle(-1),
m_DataPtr(0),
m_DataSize(0),
m_DataOffset(0)
{
    m_DataBuffer = new uint8_t[DATA_BUFFER_SIZE];
}

//------------------------------------------------------------------------------
//
StreamReader::~StreamReader()
{
    if (m_DataBuffer)
    {
        delete[] m_DataBuffer;
    }

    CloseFile();
    CloseSocket();
}

//------------------------------------------------------------------------------
//
void StreamReader::AddCallback(AcceptStreamReaderCallbacks& callback)
{
    m_Callbacks.push_back(&callback);
}

//------------------------------------------------------------------------------
//
void StreamReader::RemoveCallback(AcceptStreamReaderCallbacks& callback)
{
    std::vector<AcceptStreamReaderCallbacks*>::iterator it;
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
void StreamReader::OpenFile(const char *fileName)
{
    m_FileHandle = open(fileName, O_RDONLY, 0);
    if (m_FileHandle < 0)
    {
        throw StreamReaderException("StreamReader::OpenFile: unable to open file");
    }
}

//------------------------------------------------------------------------------
//
void StreamReader::CloseFile(void)
{
    if (m_FileHandle >= 0)
    {
        close(m_FileHandle);
        m_FileHandle = -1;
    }
}

//------------------------------------------------------------------------------
//
void StreamReader::OpenSocket(const char *multicastAddr, uint16_t port)
{
    int status;
    struct sockaddr_in saddr;
    struct ip_mreq imreq;
    uint32_t rxBufferSize = 256*1024;

    EnumerateNetworkInterfaces();
    
    // set content of struct saddr and imreq to zero
    memset(&saddr, 0, sizeof(struct sockaddr_in));
    memset(&imreq, 0, sizeof(struct ip_mreq));

    // open a UDP socket
    m_SocketHandle = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (m_SocketHandle < 0)
    {
        throw StreamReaderException("StreamReader::OpenSocket: unable to open socket");
    }
    
    // enable SO_REUSEADDR to allow multiple instances of this application to receive copies of the multicast datagrams
    int reuse = 1;
    status = setsockopt(m_SocketHandle, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse));
    if (status < 0)
    {
        throw StreamReaderException("StreamReader::OpenSocket: error setting SO_REUSEADDR socket option");
    }
    
    // increase kernel RX buffer size
    status = setsockopt(m_SocketHandle, SOL_SOCKET, SO_RCVBUF, (char *)&rxBufferSize, sizeof(rxBufferSize));
    if (status < 0)
    {
        throw StreamReaderException("StreamReader::OpenSocket: error setting SO_RCVBUF socket option");
    }

    saddr.sin_family = PF_INET;
    saddr.sin_port = htons(port);
    saddr.sin_addr.s_addr = htonl(INADDR_ANY); // bind socket to any interface
    status = bind(m_SocketHandle, (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (status < 0)
    {
        throw StreamReaderException("StreamReader::OpenSocket: error binding socket to interface");
    }
    
    std::vector<std::string>::const_iterator it;
    for (it = m_InterfaceAddresses.begin(); it != m_InterfaceAddresses.end(); ++it)
    {
        imreq.imr_multiaddr.s_addr = inet_addr(multicastAddr);
#if 1
        imreq.imr_interface.s_addr = inet_addr(it->c_str());
        std::cout << "Joining multicast " << inet_ntoa(imreq.imr_multiaddr) << " on interface " << it->c_str() << std::endl;
#else
        struct in_addr addr;
        if (inet_pton(AF_INET, it->c_str(), &addr) == 0)
        {
            throw StreamReaderException("StreamReader::OpenSocket: invalid interface address");
        }
        imreq.imr_interface.s_addr = addr.s_addr;
        std::cout << "Joining multicast " << inet_ntoa(imreq.imr_multiaddr) << " on interface " << inet_ntoa(imreq.imr_interface) << std::endl;
#endif
        // join multicast group on interface
        status = setsockopt(m_SocketHandle, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&imreq, sizeof(struct ip_mreq));

        if (status < 0)
        {
#if 1
            std::cout << "Warning: error joining multicast " << inet_ntoa(imreq.imr_multiaddr) << " on interface " << it->c_str() << std::endl;
#else
            std::stringstream ss;
            ss << "StreamReader::OpenSocket: error joining multicast group (" << std::dec << status << ")";
            throw StreamReaderException(ss.str());
#endif
        }
    }
}

//------------------------------------------------------------------------------
//
void StreamReader::CloseSocket(void)
{
    if (m_SocketHandle >= 0)
    {
        // shutdown socket
        shutdown(m_SocketHandle, 2);
        close(m_SocketHandle);
        m_SocketHandle = -1;
    }
}

//------------------------------------------------------------------------------
//
bool StreamReader::Run()
{
    bool active = ProcessData();
    
    return(active);
}

//------------------------------------------------------------------------------
//
void* StreamReader::OnThreadRun()
{
    bool active = true;
    while (active)
    {
        active = ProcessData();
    }
    
    return(NULL);
}

//------------------------------------------------------------------------------
//
bool StreamReader::ProcessData()
{
    bool active = true;
    m_DataPtr = 0;
    m_DataSize = 0;
    uint8_t *dataPtr = m_DataBuffer + m_DataOffset;
    uint32_t dataSize = 0;
    
    if (m_FileHandle >= 0)
    {
        int retVal = 0;
        int readSize = 7 * static_cast<int>(TS_PACKET_SIZE_BYTES);

#ifdef _WIN32
        retVal = _read(m_FileHandle, dataPtr, readSize);
#else // _WIN32
        retVal = read(m_FileHandle, dataPtr, readSize);
#endif // _WIN32
        if (retVal < 0)
        {
            throw StreamReaderException("StreamReader::Read: file read failed");
        }
        else if (retVal < readSize)
        {
            // reached end of file
            active = false;
        }
        else
        {
            dataSize = static_cast<uint32_t>(retVal);
        }
    }   
    else if (m_SocketHandle >= 0)
    {
        int32_t nResp;
        int32_t nOffset;
        int32_t nPayload;

        // receive packet from socket
        // int socklen = sizeof(struct sockaddr_in);
        // status = recvfrom(m_SocketHandle, dataPtr, MAX_UDP_PDU, 0, (struct sockaddr *)&saddr, &socklen);
        
        // nResp = recvfrom(m_SocketHandle, dataPtr, MAX_UDP_PDU, 0, NULL, NULL);
        nResp = recv(m_SocketHandle, reinterpret_cast<char *>(&dataPtr[0]), MAX_UDP_PDU, 0);
        if (nResp < 0)
        {
            throw StreamReaderException("StreamReader::ProcessData: error receiving data from socket");
        }
        // std::cout << "recv nResp=" << nResp << ", dataPtr[0]=" << (int)dataPtr[0] << std::endl;
        
        if (m_UseRtp)
        {
            if (!ProcessRTP(dataPtr, nResp, nOffset, nPayload))
            {
                throw StreamReaderException("StreamReader::ProcessData: RTP syntax error");             
            }           
        }
        else
        {
            nOffset = 0;
            nPayload = nResp;           
        }
        dataPtr += nOffset;
        dataSize = static_cast<uint32_t>(nPayload);
    }
    
    if (dataSize > 0)
    {
        m_DataPtr = dataPtr;
        m_DataSize = dataSize;
        InvokeCallbacks();

        m_DataOffset += dataSize;
        if (m_DataOffset > (DATA_BUFFER_SIZE - MAX_UDP_PDU))
        {
            m_DataOffset = 0;
        }
    }

    return(active);
}

//------------------------------------------------------------------------------
//
bool StreamReader::ProcessRTP(uint8_t *pPacket, int32_t nSize, int32_t &nOffset, int32_t &nPayloadSize)
//
/// RTP header parser
/// Returns TRUE if the RTP header was parsed correctly, FALSE otherwise
/// nOffset returns the offset to the beginning of the payload
/// nPayloadSize returns the size of the payload
//
//------------------------------------------------------------------------------
{
    // We don't know the payload start offset yet
    nOffset = 0;

    // Minimum header size checking
    if (nSize < MIN_RTP_HEADER)
    {
        return false;
    }
    
    // Version checking
    if (RTP_VERSION(pPacket) != EXPECTED_RTP_VERSION)
    {
        return false;
    }

    // Now, figure out header size
    nOffset = MIN_RTP_HEADER + CSRC_LEN*CSRC_COUNT(pPacket);
    if (nOffset > nSize)
    {
        return false;
    }

    // Is there an extension header?
    if (HAS_EXTENSION(pPacket))
    {
        nOffset += 2;   // 2 bytes defined by the profile
        unsigned uLen = ((unsigned)pPacket[nOffset] << 8) + (unsigned)pPacket[nOffset+1];
        nOffset += (4*uLen + 2);    // 2 bytes for the length plus the length
        if(nOffset > nSize)
        {
            return false;
        }
    }

    // Check if there is padding at the end of the packet
    if (HAS_PADDING(pPacket))
    {
        // Last byte contains the padding
        int nPadding = pPacket[nSize-1];
        if((nOffset + nPadding) > nSize)
        {
            return false;
        }
        nPayloadSize = nSize - nOffset - nPadding;
    }
    else
        nPayloadSize = nSize - nOffset;

    // Now we know
    return true;
}

//------------------------------------------------------------------------------
//
void StreamReader::InvokeCallbacks()
{
#if 0
    std::cout << "StreamReader::InvokeCallbacks (m_DataPtr=" << std::hex << (int)m_DataPtr;
    std::cout << ", m_DataSize=" << std::dec << m_DataSize << ")" << std::endl;
#endif

    std::vector<AcceptStreamReaderCallbacks*>::const_iterator it;
    for (it = m_Callbacks.begin(); it != m_Callbacks.end(); ++it)
    {
        AcceptStreamReaderCallbacks* callback = *it;
        callback->OnStreamReaderCallback(*this, 0);
    }
}

//------------------------------------------------------------------------------
//
void StreamReader::EnumerateNetworkInterfaces(void)
{
    m_InterfaceAddresses.clear();

    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == -1)
    {
        throw StreamReaderException("StreamReader::EnumerateNetworkInterfaces: getifaddrs() failed");
    }

    // Walk through linked list, maintaining head pointer so we can free list later
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;
#if 0
        // Display interface name and family (including symbolic form of the latter for the common families)
        std::cout << ifa->ifa_name << "  address family: " << family;
        switch (family)
        {
        case AF_PACKET:
            std::cout << " (AF_PACKET)";
            break;
        case AF_INET:
            std::cout << " (AF_INET)";
            break;
        case AF_INET6:
            std::cout << " (AF_INET6)";
            break;
        }
        std::cout << std::endl;
#endif

        // For an AF_INET* interface address, display the address
        if (family == AF_INET || family == AF_INET6)
        {
            s = getnameinfo(ifa->ifa_addr,
                   (family == AF_INET) ? sizeof(struct sockaddr_in) :
                                         sizeof(struct sockaddr_in6),
                   host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0)
            {
                throw StreamReaderException("StreamReader::EnumerateNetworkInterfaces: getnameinfo() failed");
            }
#if 0
            std::cout << "    address: <" << host << ">" << std::endl;
#endif
            if (family == AF_INET && (strcmp(ifa->ifa_name, "eth0") == 0))
            {
                std::cout << "Using interface " << ifa->ifa_name << " (" << host << ")" << std::endl;
                m_InterfaceAddresses.push_back(host);
            }
        }
    }

    freeifaddrs(ifaddr);
}
