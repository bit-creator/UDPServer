#ifndef UDP_SERVER_DATA_STORAGE_H
#define UDP_SERVER_DATA_STORAGE_H

#include "config.h"
#include <cstdint>
#include <array>
#include <memory>
#include <vector>

// compile time analog to std::bit_ceil();
constexpr uint32_t bit_ceil(uint32_t n) noexcept
{
    if ((n & (n - 1)) == 0) 
        return n;

    uint32_t result = 1;
    
    while (n != 0)
    {
        n >>= 1;
        result <<= 1;
    }
    
    return result;
}

using Storage = std::shared_ptr<std::array<double, GENERATOR_THRESHOLD>>;
using CStorage = std::shared_ptr<const std::array<double, GENERATOR_THRESHOLD>>;

class DataStorage
{
    static constexpr float    loadFactor = 0.5;
    static constexpr uint64_t numOfDoubles = GENERATOR_THRESHOLD;
    static constexpr uint64_t sizeOfHashtable = bit_ceil((uint64_t)(numOfDoubles / loadFactor));
    static constexpr uint32_t sizeMinusOne = sizeOfHashtable - 1;
    static constexpr uint32_t tombstone = sizeOfHashtable + 1;

    // array-of-structures -> structure-of-arrays optimization
    Storage                                m_Storage;
    std::array<uint32_t, sizeOfHashtable>  m_Offsets;
    std::array<uint32_t, sizeOfHashtable>  m_Nexts;

    std::vector<std::pair</*offset*/uint32_t, /*next or tombstone*/uint32_t>>  m_Collisions;
    
    uint32_t    m_counter = 0;

    private:
        void init()
        {
            m_Offsets.fill(tombstone);
            m_Nexts.fill(tombstone);
        }

    public:
        DataStorage()
        {
            m_Storage = std::make_shared<std::array<double, GENERATOR_THRESHOLD>>();
            init();
        }

        bool insert(double value)
        {
            if(m_counter == numOfDoubles) return false;
            uint32_t pos = *reinterpret_cast<uint64_t*>(&value) & sizeMinusOne;
            if(m_Offsets[pos] == tombstone) [[likely]]
            {
                m_Offsets[pos] = m_counter;
                (*m_Storage)[m_counter] = value;
                ++m_counter;
                return true;
            }
            else if((*m_Storage)[m_Offsets[pos]] == value) // value already exist
            {
                return false;
            }
            else if(m_Nexts[pos] == tombstone)
            {
                (*m_Storage)[m_counter] = value;
                m_Nexts[pos] = m_Collisions.size();
                m_Collisions.push_back({m_counter, tombstone});
                ++m_counter;
                return true;
            }
            else [[unlikely]]
            {
                // walking throw collision sequence
                pos = m_Nexts[pos];
                while(m_Collisions[pos].second != tombstone)
                {
                    if((*m_Storage)[m_Collisions[pos].first] == value)
                        return false;
                    else
                        pos = m_Collisions[pos].second;
                }
                (*m_Storage)[m_counter] = value;
                m_Collisions[pos].second = m_Collisions.size();
                m_Collisions.push_back({m_counter, tombstone});
                ++m_counter;
                return true;
            }

        }

        uint32_t size() const
        {
            return m_counter;
        }

        const double* data() const
        {
            return (*m_Storage).data();
        }

        CStorage getUnderlying() const
        {
            return m_Storage;
        }
};

#endif // UDP_SERVER_DATA_STORAGE_H