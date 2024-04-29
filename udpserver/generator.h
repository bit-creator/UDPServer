#ifndef UDP_SERVER_GENERATOR_H
#define UDP_SERVER_GENERATOR_H

#include "datastorage.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

using SubmitCallback = std::function<void(CStorage)>;
using Timestamp      = std::chrono::time_point<std::chrono::steady_clock>;

struct Job;

class Generator
{
    std::vector<std::shared_ptr<Job>>      m_Jobs;

    public:
        Generator(uint32_t numOfThreads);
    
        void addNewInstance(double seed, SubmitCallback ready);
};

#endif // UDP_SERVER_GENERATOR_H