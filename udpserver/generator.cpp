#include "generator.h"
#include "datastorage.h"
#include <chrono>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include "config.h"

struct AssociatedInfo
{
    std::uniform_real_distribution<double>  spawn;
    std::shared_ptr<DataStorage>            storage;
    Timestamp                               timestamp;
    SubmitCallback                          ready;
};

struct Job
{
    std::vector<AssociatedInfo>         instances;
    std::mutex                          mutex;
    std::mt19937                        engine = std::mt19937(std::random_device{}());

    void addNewInstance(double seed, SubmitCallback ready)
    {
        std::lock_guard _(mutex);

        instances.push_back
        ({
            std::uniform_real_distribution<double>(-seed, seed),
            std::make_shared<DataStorage>(),
            std::chrono::steady_clock::now(),
            std::move(ready)
        });
    }

    uint32_t getPayload()
    {
        std::lock_guard _(mutex);
        return instances.size();
    }
};

Generator::Generator(uint32_t numOfThreads)
{
    for(uint32_t i = 0; i < numOfThreads; ++i)
    {
        m_Jobs.push_back(std::make_shared<Job>());
        std::thread([](std::weak_ptr<Job> wjob)
        {
            Timestamp oldest;

            while(auto job = wjob.lock())
            {
                std::lock_guard _(job->mutex);
                for(auto iter = job->instances.begin(); iter != job->instances.end();)
                {
                    auto& instance = *iter;

                    if (oldest.time_since_epoch().count() == 0 || instance.timestamp < oldest)
                        oldest = instance.timestamp;

                    uint32_t num = 10; // TODO use info about timestamp to increase payload to oldest instances
                    
                    for(uint32_t i = 0; i < num; ++i)
                        instance.storage->insert(instance.spawn(job->engine));

                    if(instance.storage->size() >= GENERATOR_THRESHOLD)
                    {
                        if(oldest == instance.timestamp)
                            oldest = Timestamp();

                        instance.ready(instance.storage->getUnderlying());

                        iter = job->instances.erase(iter);
                    }
                    else
                    {
                        ++iter;
                    }

                }
            }
        }, m_Jobs.back()).detach();
    }
}

void Generator::addNewInstance(double seed, SubmitCallback ready)
{
    uint32_t min = 0;
    uint32_t minPayload = m_Jobs[min]->getPayload();
    for(uint32_t i = 1; i < m_Jobs.size(); ++i)
    {
        uint32_t currentPayload = m_Jobs[i]->getPayload();
        if(minPayload > currentPayload)
        {
            min = i;
            minPayload = currentPayload;
        }
    }

    m_Jobs[min]->addNewInstance(seed, std::move(ready));
}
