#ifndef UDP_SERVER_SUBMIT_INFO_H
#define UDP_SERVER_SUBMIT_INFO_H


#include "datastorage.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/udp.hpp>
#include <memory>
#include <vector>

using boost::asio::const_buffer;
using boost::asio::ip::udp;

class SubmitInfo
{
    CStorage                       m_Storage;
    std::vector<const_buffer>      m_Pages;
    std::vector<std::byte>         m_Checksums;
    std::shared_ptr<udp::endpoint> m_Destination;

    private:
        void paginate();
        void genChecksums();

    public:
        SubmitInfo(CStorage storage, std::shared_ptr<udp::endpoint> dst);
        
        const std::vector<const_buffer>& pages() const { return m_Pages; }
        const std::vector<std::byte>& checksums() const { return m_Checksums; }
        std::shared_ptr<udp::endpoint> dst() const {return m_Destination; }
};

using SubmitQueue = std::vector<std::shared_ptr<SubmitInfo>>;

#endif // UDP_SERVER_SUBMIT_INFO_H