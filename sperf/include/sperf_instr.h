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
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <dirent.h>

#define RED     			"\x1b[31m"
#define GREEN   			"\x1b[32m"
#define YELLOW  		"\x1b[33m"
#define BLUE    			"\x1b[34m"
#define MAGENTA 		"\x1b[35m"
#define CYAN   			"\x1b[36m"
#define RESET   			"\x1b[0m"

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
    void FindEnclosures(std::string& txt, std::string e1, std::string e2, std::vector<commentRegion> &commentRegions);
    bool isInsidComment(unsigned long long int p, std::vector<commentRegion> &commentRegions);
private:
    std::string buffer, dpath;
    std::vector<std::string> extensions, files;
    std::ifstream abre;
    DIR *dp;
    dirent *dptr;
};

#endif // SPERF_INSTR_H_INCLUDED
