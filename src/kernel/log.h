/*  CryptoKernel - A library for creating blockchain based digital currency
    Copyright (C) 2016  James Lovejoy

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

#include <string>
#include <fstream>
#include <mutex>

#define LOG_LEVEL_ERR 1
#define LOG_LEVEL_WARN 2
#define LOG_LEVEL_INFO 3

namespace CryptoKernel {

/**
* The log class provides basic logging capabilities including printing messages
* to the screen and write to a log file. Automatically adds timestamps and
* message severity level.
*/
class Log {
public:
    /**
    * Constructs a log instance using the given filename. Optionally prints
    * log messages to the console. This will either create the log file or
    * lock the existing log file for writing and add newlines to the file
    * before starting the log, continuing from the last line already in the
    * log file.
    *
    * @param filename optional filename of the log file. Uses "CryptoKernel.log" by default
    * @param printToConsole optional flag, prints log messages to the console when true.
             Defaults to false.
    */
    Log(const std::string filename = "CryptoKernel.log", const bool printToConsole = false);

    /**
    * Standard destructor. Should ensure messages are flushed to the log file and
    * release the file for writing by other processes.
    */
    ~Log();

    /**
    * Adds a message to the log with a given severity, leaving a new line in the
    * following format:
    *
    * YYYY-MM-DD HH:MM:SS ERROR|WARNING|INFO Log message
    *
    * If the print to console flag was set to true in the constructor the message
    * is also printed to the console.
    *
    * @param loglevel the severity of the message, can either be LOG_LEVEL_ERR (1), LOG_LEVEL_WARN (2)
    *        or LOG_LEVEL_INFO (3)
    * @param message the message string of the log entry
    * @return true iff the message was logged successfully, otherwise false
    * @throw std::runtime_error if the loglevel is an error
    */
    bool printf(const int loglevel, const std::string message);

    /**
    * Checks the status of the log class
    *
    * @return true iff there are no internal errors and it is safe to continue
    */
    bool getStatus();

private:
    bool fPrintToConsole;
    bool status;
    std::ofstream logfile;
    std::mutex logfilemutex;
};

}

#endif // LOG_H_INCLUDED

