#ifndef UDP_SERVER_UDP_SERVER_H
#define UDP_SERVER_UDP_SERVER_H

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/strand.hpp>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <thread>
#include "../common/logger.h"
#include "generator.h"
#include "submitinfo.h"

using boost::asio::ip::udp;
using boost::asio::const_buffer;
using boost::system::error_code;
using boost::asio::steady_timer;

struct Index;

class Server
{
    boost::asio::io_context m_Context;
    std::jthread            m_InOutThread;
    uint16_t                m_Port;
    udp::socket             m_Socket;

    boost::asio::io_context::strand m_Strand;

    std::shared_ptr<Logger>     m_Log;

    SubmitQueue              m_submitQueue;
    std::shared_mutex        m_submitQMtx;

    Generator               m_Generator;
    
    std::byte               m_Buffer[65515/*max possible udp packet*/];

    private:
        bool validate(std::shared_ptr<udp::endpoint> endpoint, double seed);
        void receive();
        void submit(std::shared_ptr<udp::endpoint> dst, const std::vector<const_buffer>& pages, std::function<Index()> nextIdx);
        void submit(std::shared_ptr<udp::endpoint> dst, const std::vector<std::byte>& checksums);

        // helpers
        void processNewConnection(std::shared_ptr<udp::endpoint> dst);
        void resubmitChecksums(std::shared_ptr<udp::endpoint> dst, uint32_t recvd);
        void resubmitLost(std::shared_ptr<udp::endpoint> dst, uint32_t recvd);
        void forget(std::shared_ptr<udp::endpoint> dst);

    public:
        Server(uint16_t port);
        void runLoop();
};

#endif