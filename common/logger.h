#ifndef UDP_SERVER_LOGGER_H
#define UDP_SERVER_LOGGER_H

#include <chrono>
#include <ctime>
#include <iostream>
#include <fstream>
#include <boost/system/error_code.hpp>
#include <string>

class Logger
{
    protected:
        std::time_t now()
        {
            return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        }

    public:
        virtual bool log(const boost::system::error_code&) = 0;
        virtual void log(const std::string&) = 0;
};

class FileLogger : public Logger
{
    std::ofstream   m_Logfile;

public:
    FileLogger(const std::string& name)
        : m_Logfile(name, std::ios::app)
    {

    }

    bool log(const boost::system::error_code& error) override
    {
        if(error.failed())
            m_Logfile << now() << " | " << error.message() << std::endl;
        return error.failed();
    }

    void log(const std::string& message) override
    {
        m_Logfile << now() << " | " << message << std::endl;
    }
};

class CoutLogger : public Logger
{
public:
    bool log(const boost::system::error_code& error) override
    {
        if(error.failed())
            std::cout << now() << " | " << error.message() << std::endl;
        return error.failed();
    }

    void log(const std::string& message) override
    {
        std::cout << now() << " | " << message << std::endl;
    }
};

#endif // UDP_SERVER_LOGGER_H