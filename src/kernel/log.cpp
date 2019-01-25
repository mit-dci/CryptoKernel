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

#include <ctime>
#include <chrono>
#include <sstream>
#include <iostream>

#include "log.h"
#include "version.h"

CryptoKernel::Log::Log(std::string filename, bool printToConsole) {
    fPrintToConsole = printToConsole;
    logfile.open(filename, std::ios::app);
    if(logfile.is_open()) {
        logfile << "\n\n\n\n\n";
        printf(LOG_LEVEL_INFO, "CryptoKernel version " + version + " started");
        status = true;
    } else {
        status = false;
    }
}

CryptoKernel::Log::~Log() {
    logfilemutex.lock();
    logfile.close();
    logfilemutex.unlock();
}

bool CryptoKernel::Log::printf(int loglevel, std::string message) {
    std::chrono::system_clock::time_point today = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(today);
    logfilemutex.lock();
    std::string t(ctime(&tt));
    logfilemutex.unlock();
    std::ostringstream stagingstream;
    stagingstream << t.substr(0, t.length() - 1) << " ";

    switch(loglevel) {
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

    logfilemutex.lock();
    if(fPrintToConsole) {
        std::cout << stagingstream.str() << std::flush;
    }

    logfile << stagingstream.str();
    logfilemutex.unlock();

    if(loglevel == LOG_LEVEL_ERR) {
        throw std::runtime_error("Fatal error");
    }

    return true;
}

bool CryptoKernel::Log::getStatus() {
    return status;
}

