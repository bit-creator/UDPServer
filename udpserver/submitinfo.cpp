#include "submitinfo.h"
#include "config.h"
#include <cmath>

SubmitInfo::SubmitInfo(CStorage storage, std::shared_ptr<udp::endpoint> dst)
    : m_Storage(std::move(storage))
    , m_Destination(std::move(dst))
{ 
    paginate();
    genChecksums();
}

void SubmitInfo::paginate()
{
    const double* data = m_Storage->data();
    uint32_t size = PAGE_SIZE;
    uint32_t totalSize = m_Storage->size() * sizeof(double);
    uint32_t loadedSize = PAGE_SIZE;

    std::generate_n(std::back_inserter(m_Pages), ceil(totalSize / (float)PAGE_SIZE), [&]()
    {
        auto res = const_buffer(data, size);
        size = std::min((uint32_t)PAGE_SIZE, totalSize - loadedSize);
        data += PAGE_SIZE / 8;
        loadedSize += size;
        return res;
    });
}

/*
 * Check sums scheme:
 * first 2 bytes - size of single page
 * than  8 bytes sequence of unique identifier for every page
*/
void SubmitInfo::genChecksums()
{
    m_Checksums.resize(m_Pages.size() * sizeof(double) + 2);
    memcpy(m_Checksums.data(), &PAGE_SIZE, 2);
    uint32_t shift = 2;
    for(const auto& page: m_Pages)
    {
        // I use first value in page "like" checksum, because actually
        // all value are unique and than any false positive impossible...
        memcpy(m_Checksums.data() + shift, page.data(), 8);
        shift += 8;
    }
}

