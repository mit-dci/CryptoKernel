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

