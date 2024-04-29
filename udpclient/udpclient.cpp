#include "udpclient.h"
#include <algorithm>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <execution>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>

UDPClient::UDPClient(double seed, std::string dest, uint16_t port, std::string output)
    : m_Seed(seed)
    , m_Response(ceil((float)MAX_PAGE_SIZE / sizeof(double)), 0.0)
    , m_Context()
    , m_Socket(m_Context, udp::endpoint(udp::v6(), 0))
    , m_Server(boost::asio::ip::address::from_string(dest), port)
    , m_Delay(m_Context)
    , m_Worker([this](){sortAndWrite();})
    , m_output(output)
    , m_Log(std::make_shared<FileLogger>(m_output + ".log"))
{
    pushSeed();
    receive(m_Response.data(), MAX_PAGE_SIZE);
    m_Context.run();
}

void UDPClient::pushSeed()
{
    m_Delay.expires_after(std::chrono::seconds(3));
    m_Delay.async_wait([=, this](const boost::system::error_code& error)
    {
        m_Log->log(error);
        m_Socket.send_to(boost::asio::buffer(&m_Seed, 8), m_Server);
    });
}

void UDPClient::receive(double* shift, uint32_t size)
{
    m_Socket.async_receive_from(boost::asio::buffer(shift, size), m_Server,
    [=, this](const boost::system::error_code& error, std::size_t recvd)
    {
        m_Log->log(error);
        if(recvd == 1) // opCode
        {
            processPing();
            if(!m_allDataReached)
                receive(shift, size);
        }
        else if((recvd - 2) % 8 == 0) // checksum branch
        {
            processChecksums(shift, size, recvd);
            if(!m_allDataReached)
            {
                double* newShift = m_Response.data() + m_ReceivedPages.size() * (m_PageSize / sizeof(double));
                receive(newShift, (&*m_Response.end() - newShift) * sizeof(double));
            }
        }
        else if(recvd % 8 == 0) // data branch
        {
            processData(shift, size, recvd);
        }
        else // unknown branch
        {
            std::string msg;
            memcpy(&msg, shift, recvd);
            m_Log->log(msg);
        }
    });
}

void UDPClient::sortAndWrite()
{
    m_DataReady.wait(false);

    std::sort(std::execution::par_unseq, m_Response.begin(), m_Response.end(), [](double a, double b){return a > b;});

    std::ofstream outFile(m_output, std::ios::out | std::ios::binary);
    outFile.write((char*)m_Response.data(), sizeof(double) * m_Response.size());
}

void UDPClient::waitUntilEnd()
{
    if(m_Worker.joinable())
        m_Worker.join();
}

void UDPClient::processChecksums(double* shift, uint32_t size, uint32_t recvd)
{
    uint32_t pagesCount = (recvd - 2) / 8;

    m_Checksums.resize(pagesCount);

    memcpy(&m_PageSize, shift, 2);
    memcpy(m_Checksums.data(), (char*)shift + 2, 8 * pagesCount);
    m_ChecksumsReceived = true;

    m_Response.resize(pagesCount * (m_PageSize / 8));

    if(m_ReceivedPages.size() == pagesCount)
    {
        pingBack();
        m_DataReady.test_and_set();
        m_DataReady.notify_one();
    }
}

void UDPClient::processData(double* shift, uint32_t size, uint32_t recvd)
{
    if(m_ChecksumsReceived)
    {
        if(size > m_PageSize)
        {
            receive(shift + recvd / 8, size - recvd);
        }
        else if(size == m_PageSize)
        {
            pingBack();
            if(recvd != m_PageSize) // means tail smaller than full page
                m_Response.erase(m_Response.begin() + ((m_Checksums.size() - 1) * m_PageSize + recvd / 8), m_Response.end());
            m_DataReady.test_and_set();
            m_DataReady.notify_one();
        }
    }
    else
    {
        // always keep free at least MAX_PAGE_SIZE free space
        m_Response.resize(m_Response.size() + recvd / sizeof(double));
        double* newShift = m_Response.data() + m_ReceivedPages.size() * (recvd / sizeof(double));
        receive(newShift + recvd / 8, MAX_PAGE_SIZE);
    }

    m_ReceivedPages.emplace(*shift);
}

void UDPClient::processPing()
{
    std::vector<std::byte> returnBuffer;

    if(m_ChecksumsReceived)
    {
        if(m_Checksums.size() == m_ReceivedPages.size())
            pingBack();
        else
            returnBuffer = missedPages();
    }
    else
    {
        returnBuffer = receivedChecksums();
    }
    m_Socket.send_to(boost::asio::buffer(returnBuffer.data(), returnBuffer.size()), m_Server);
}

void UDPClient::pingBack()
{
    
    m_allDataReached = true;
    uint8_t end;
    m_Socket.send_to(boost::asio::buffer(&end, 1), m_Server);
}

std::vector<std::byte> UDPClient::missedPages()
{
    std::vector<std::byte> returnBuffer;
    std::vector<uint16_t> idx;
    for(uint32_t i = 0; i < m_Checksums.size(); ++i)
        if(!m_ReceivedPages.contains(m_Checksums[i]))
            idx.push_back(i);

    returnBuffer.resize(idx.size() * 2 + 1/*padding*/);
    memcpy(returnBuffer.data(), idx.data(), idx.size() * 2);
    return returnBuffer;
}

std::vector<std::byte> UDPClient::receivedChecksums()
{
    std::vector<std::byte> returnBuffer;
    std::vector<double> existedPages(m_ReceivedPages.begin(), m_ReceivedPages.end());
    returnBuffer.resize(existedPages.size() * sizeof(double) + 2/*padding*/);
    memcpy(returnBuffer.data(), existedPages.data(), existedPages.size() * sizeof(double));
    return returnBuffer;
}