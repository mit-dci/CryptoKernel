#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include <string>
#include <fstream>
#include <mutex>

#define LOG_LEVEL_ERR 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_INFO 3

namespace CryptoKernel
{

class Log
{
    public:
        Log(std::string filename = "CryptoKernel.log", bool printToConsole = false);
        ~Log();
        bool printf(int loglevel, std::string message);
        bool getStatus();

    private:
        bool fPrintToConsole;
        bool status;
        std::ofstream logfile;
        std::mutex logfilemutex;
};

}

#endif // LOG_H_INCLUDED

