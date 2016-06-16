#include <ctime>
#include <chrono>
#include <sstream>
#include <iostream>

#include "log.h"
#include "version.h"

CryptoKernel::Log::Log(std::string filename, bool printToConsole)
{
    fPrintToConsole = printToConsole;
    logfile.open(filename, std::ios::app);
    if(logfile.is_open())
    {
        logfile << "\n\n\n\n\n";
        printf(LOG_LEVEL_INFO, "CryptoKernel version " + version + " started");
        status = true;
    }
    else
    {
        status = false;
    }
}

CryptoKernel::Log::~Log()
{
    logfilemutex.lock();
    logfile.close();
    logfilemutex.unlock();
}

bool CryptoKernel::Log::printf(int loglevel, std::string message)
{
    std::chrono::system_clock::time_point today = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(today);
    std::string t(ctime(&tt));
    std::ostringstream stagingstream;
    stagingstream << t.substr(0, t.length() - 1) << " ";

    switch(loglevel)
    {
        case LOG_LEVEL_ERR:
            stagingstream << "ERROR ";
            break;

        case LOG_LEVEL_WARN:
            stagingstream << "WARNING ";
            break;

        case LOG_LEVEL_INFO:
            stagingstream << "INFO ";
            break;

        default:
            return false;
            break;
    }

    stagingstream << message << "\n";

    if(fPrintToConsole)
    {
        std::cout << stagingstream.str();
    }

    logfilemutex.lock();
    logfile << stagingstream.str();
    logfile.flush();
    logfilemutex.unlock();

    return true;
}

bool CryptoKernel::Log::getStatus()
{
    return status;
}

