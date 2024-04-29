#ifndef UDP_SERVER_UDP_CLIENT_H
#define UDP_SERVER_UDP_CLIENT_H

#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <cstdint>
#include <memory>
#include <set>
#include <thread>
#include "../common/logger.h"

using boost::asio::ip::udp;

inline constexpr uint16_t MAX_PAGE_SIZE = 65'515; // max possible payload

class UDPClient
{
    double                 m_Seed;
    std::vector<double>    m_Response;
    
    std::vector<double>    m_Checksums;
    std::set<double>       m_ReceivedPages;
    uint16_t               m_PageSize;
    bool                   m_ChecksumsReceived = false;
    bool                   m_allDataReached = false;

    std::atomic_flag       m_DataReady;
    std::jthread           m_Worker;
    std::string            m_output;

    boost::asio::io_context     m_Context;
    udp::socket                 m_Socket;
    udp::endpoint               m_Server;
    boost::asio::steady_timer   m_Delay;

    std::shared_ptr<Logger>     m_Log;                      

    private:
        void receive(double* shift, uint32_t size);
        void pushSeed();
        void sortAndWrite();

        // helpers
        void processPing();
        void pingBack();
        std::vector<std::byte> missedPages();
        std::vector<std::byte> receivedChecksums();
        void processChecksums(double* shift, uint32_t size, uint32_t recvd);
        void processData(double* shift, uint32_t size, uint32_t recvd);

    public:
        UDPClient(double seed, std::string dest, uint16_t port, std::string output);
        void waitUntilEnd();
};

#endif // UDP_SERVER_UDP_CLIENT_H