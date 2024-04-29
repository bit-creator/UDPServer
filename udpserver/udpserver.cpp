#include "udpserver.h"
#include "config.h"
#include "datastorage.h"
#include <algorithm>
#include <boost/asio/buffer.hpp>
#include <boost/asio/buffered_stream.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>
#include <optional>
#include "submitinfo.h"

struct Index
{
    uint32_t  index;
    bool      last = false; // sentinel for last one idx

    operator uint32_t()
    {
        return index;
    }
};

Server::Server(uint16_t port)
    : m_Context()
    , m_Port(port)
    , m_Socket(m_Context, {udp::v6(), m_Port})
    , m_Strand(m_Context)
    , m_Generator(std::thread::hardware_concurrency() - 1)
    , m_Log(std::make_shared<FileLogger>("server.log"))
{
    m_InOutThread = std::jthread([this]()
    {
        receive();
        m_Context.run();
    });
}

void Server::receive()
{
    auto sender = std::make_shared<udp::endpoint>();

    m_Socket.async_receive_from(boost::asio::buffer(m_Buffer, 65515), *sender,
        [=, this](error_code ec, uint64_t recvd)
        {
            m_Log->log(ec);
            if(recvd == sizeof(double))
                processNewConnection(std::move(sender));

            else if(recvd == 1) // just means client successfully receive all data
                forget(std::move(sender));

            else if(recvd % 2 == 1) // page indexes
                resubmitLost(std::move(sender), recvd - 1);

            else if(recvd % 2 == 0) // received checksums
                resubmitChecksums(std::move(sender), recvd - 2);

            receive();
        }
    );
}

void Server::runLoop()
{
    if(m_InOutThread.joinable())
        m_InOutThread.join();
}

bool/*is valid*/ Server::validate(std::shared_ptr<udp::endpoint> endpoint, double seed)
{
    std::optional<std::string> error;

    if(endpoint->protocol().family() != m_Socket.local_endpoint().protocol().family())
        error = "Protocol mismatch";

    // because we use range [-X; X] enough have at least GENERATOR_THRESHOLD / 2 epsilons in seed 
    if(seed < std::numeric_limits<double>::epsilon() * ceil(GENERATOR_THRESHOLD / 2.))
        error = "seed very small";

    // other checks...

    if(error)
    {
        std::string msg = *error;
        if(msg.size() % 2 == 0)
            msg.push_back(' '/*padding just to ensure client not recognize data and fall in error*/);
        m_Socket.send_to(const_buffer(error->data(), error->size()), *endpoint);
    }

    return !error.has_value();
}

void Server::submit(std::shared_ptr<udp::endpoint> dst, const std::vector<const_buffer>& pages, std::function<Index()> nextIdx)
{
    Index idx = nextIdx();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    m_Socket.async_send_to(pages[idx], *dst,
        m_Strand.wrap([=, this](const error_code& error, std::size_t transferred) 
        {
            m_Log->log(error);
            if(!idx.last)
                submit(std::move(dst), pages, std::move(nextIdx));
            else
                m_Socket.send_to(boost::asio::buffer(&m_Buffer, 1), *dst);
        }
    ));
}

void Server::submit(std::shared_ptr<udp::endpoint> dst, const std::vector<std::byte>& checksums)
{
    m_Socket.async_send_to(const_buffer(checksums.data(), checksums.size()), *dst,
        m_Strand.wrap([this](const error_code& error, std::size_t transferred) { m_Log->log(error);}));
}

void Server::processNewConnection(std::shared_ptr<udp::endpoint> dst)
{
    double seed;
    memcpy(&seed, m_Buffer, sizeof(double));

    if(!validate(dst, seed))
        return;

    auto submitCallback = [=, this](CStorage storage)
    {
        auto info = std::make_shared<SubmitInfo>(std::move(storage), dst);

        {
            std::unique_lock _(m_submitQMtx);
            m_submitQueue.push_back(info);
        }

        std::shared_lock _(m_submitQMtx);

        submit(dst, info->checksums());

        submit(dst, info->pages(),
        [i = 0u, end = info->pages().size()]() mutable -> Index
        { return {i, ++i == end}; });
    };

    m_Generator.addNewInstance(seed, std::move(submitCallback));
}

void Server::resubmitChecksums(std::shared_ptr<udp::endpoint> dst, uint32_t recvd)
{
    std::vector<double> cs;
    cs.resize((recvd - 2/*padding*/) / 8);
    memcpy(cs.data(), m_Buffer, recvd - 2/*padding*/);

    std::shared_lock _(m_submitQMtx);

    if(m_submitQueue.empty())
    {
        m_Log->log("submit queue empty");
        return;
    }

    const auto& info = *std::find_if(m_submitQueue.begin(), m_submitQueue.end(),
        [=](const auto& val) { return val->dst() == dst; });

    submit(dst, info->checksums());

    std::vector<uint16_t> idx;

    auto checksums = std::span<double>((double*)(info->checksums().data() + 2), (info->checksums().size() - 2) / 8);

    for(uint32_t i = 0; i < checksums.size(); ++i)
    {
        if(std::find(cs.begin(), cs.end(), checksums[i]) != cs.end())
            idx.push_back(i);
    }

    submit(std::move(dst), info->pages(), [i = 0u, ind = idx]() mutable -> Index 
    {
        return {ind[i], ++i == ind.size()};
    });
}

void Server::resubmitLost(std::shared_ptr<udp::endpoint> dst, uint32_t recvd)
{
    std::vector<uint16_t> idx;
    idx.resize((recvd - 1/*padding*/) / 2);
    memcpy(idx.data(), m_Buffer, recvd - 1/*padding*/);

    std::shared_lock _(m_submitQMtx);

    if(m_submitQueue.empty())
    {
        m_Log->log("submit queue empty");
        return;
    }

    const auto& info = *std::find_if(m_submitQueue.begin(), m_submitQueue.end(),
        [=](const auto& val) { return val->dst() == dst; });

    submit(std::move(dst), info->pages(), [i = 0u, ind = idx]() mutable -> Index 
    {
        return {ind[i], ++i == ind.size()};
    });
}

void Server::forget(std::shared_ptr<udp::endpoint> dst)
{
    std::unique_lock _(m_submitQMtx);
    if(!m_submitQueue.empty())
        auto iter = std::remove_if(m_submitQueue.begin(), m_submitQueue.end(),
            [=](const auto& val) { return val->dst() == dst; });
}
