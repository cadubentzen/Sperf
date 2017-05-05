#ifndef SPERF_INSTR_H_INCLUDED
#define SPERF_INSTR_H_INCLUDED

#pragma once
#include <string>
#include <fstream>
#include <vector>
#include <dirent.h>

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
