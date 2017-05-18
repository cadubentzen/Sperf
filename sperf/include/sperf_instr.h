/*
    Copyright (C) 2017 Márcio Jales, Vitor Ramos

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

#ifndef SPERF_INSTR_H_INCLUDED
#define SPERF_INSTR_H_INCLUDED

#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <dirent.h>

std::string intToString(int x);
int stringToInt(std::string x);

class Instrumentation
{
public:
    struct commentRegion
    {
        unsigned li, lf;
    };
    void read_config_file(std::string file_name);
    void getFileNames();
    void instrument();
private:
    std::string buffer, dpath;
    std::vector<std::string> extensions, files;
    std::ifstream abre;
    DIR *dp;
    dirent *dptr;
};

#endif // SPERF_INSTR_H_INCLUDED
