/*
    Copyright (C) 2017 Márcio Jales

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <assert.h>

#include <sys/stat.h>
#include <dirent.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

using namespace std;

#include "sperfops.h"
#include "sperf_instr.h"

#define RED     			"\x1b[31m"
#define GREEN   			"\x1b[32m"
#define YELLOW  		"\x1b[33m"
#define BLUE    			"\x1b[34m"
#define MAGENTA 		"\x1b[35m"
#define CYAN   			"\x1b[36m"
#define RESET   			"\x1b[0m"


class Sperf
{
private:
    enum PerfConfig { SET_THREADS, SET_PIPE };
    enum PathConfig { ETC_PATH, RESULT_PATH };
public:
    Sperf(char* argv[], int argc);
    void run();
private:
    string get_perfpath(string argmnt, Sperf::PathConfig op);
    void set_perfcfg(int val, Sperf::PerfConfig op);

    void config_menu(char* argv[], int argc);
    void config_output(string path);
    void read_config_file(string exec_path);

    void store_time_information();
    void store_time_information_csv();

private:
    struct proc_info
    {
        double start, end;

        uint current_arg;
        uint cur_exec, num_args;
        char** args;

        map<int, s_info> info;
    };
    map<uint, proc_info> info_thr_proc;
    int pipes[2];/* pipes[0] = leitura; pipes[1] = escrita */
    uint optset = 0;
    uint num_exec= 0, num_args= 0;
    char** args;
    string config_file, csv_file, program_name, result_file;
    vector<uint> list_of_threads_value;
    vector<char**> list_of_args;
    vector<uint> list_of_args_num;
    bool out_csv= false;
    ofstream out;
};

int main(int argc, char *argv[])
{
    try
    {
        if(string(argv[1]) == "-i")
        {
            Instrumentation inst;
            if(argc == 2)
                throw  "missing arguments to instrumentation";
            inst.read_config_file(argv[2]);
            inst.getFileNames();
            inst.instrument();
        }
        else
        {
            Sperf sperf(argv, argc);
            sperf.run();
        }
    }
    catch(const char* e)
    {
        cerr << RED "[Sperf]" RESET << " "  << e << endl;
    }
    catch(string e)
    {
        cerr << RED "[Sperf]" RESET << " " << e << endl;
    }
    return 0;
}

Sperf::Sperf(char* argv[], int argc)
{
    config_menu(argv, argc);

    config_output(argv[0]);

    read_config_file(argv[0]);
}

// set environment variables to control number of threads
// or set pipe
void Sperf::set_perfcfg(int val, Sperf::PerfConfig op)
{
    string str= intToString(val);
    if (op == SET_THREADS)
    {
        setenv("NUM_THRS_ATUAL", str.c_str(), 1);
        setenv("OMP_NUM_THREADS", str.c_str(), 1);
    }
    else if (op == SET_PIPE)
        setenv("FD_PIPE", str.c_str(), 1);
    else
        throw  " Invalid option in set_perfcfg";
}

// get program path
string Sperf::get_perfpath(string argmnt, Sperf::PathConfig op)
{
    string path;
    path=argmnt.substr(0,argmnt.find_last_of("/"));

    if (op == ETC_PATH)
        path+="/../etc/"+string(config_file)+".conf";
    // add the result path
    if (op == RESULT_PATH)
        path+="/../results/";

    return path;
}

// program configurations
void Sperf::config_menu(char* argv[], int argc)
{
    int i=0;
    bool thrnum_req= false;
    args = (char **) malloc((argc + 1)*sizeof(char*));
    do
    {
        if(string(argv[i]) == "-t") // set number of the argument that will pass the number of thread to the program
        {
            thrnum_req= true;
            i++;
            if(i<argc) // cheking if the user pass the argument
                optset = stringToInt(argv[i]);
        }
        else if (string(argv[i]) == "-c") // passing the config file path
        {
            i++;
            if(i<argc) // cheking if the user pass the argument
                config_file= argv[i];
        }
        else if(string(argv[i]) == "-o")
        {
            out_csv= true;
            i++;
            if(i<argc) // cheking if the user pass the argument
                csv_file= argv[i];
        }
        else // other arguments
        {
            args[num_args] = (char *)malloc(250*sizeof(char));
            strcpy(args[num_args], argv[i]);
            num_args++;
        }
        i++;
    } while (i < argc);


    args[num_args] = (char *) NULL;

    if(thrnum_req)
        cout << BLUE "[Sperf]" RESET " sperf_thrnum function required";
    else
        cout << BLUE "[Sperf]" RESET " Thread value passed by command line argument to the target application. sperf_thrnum function not required\n";

    if(num_args == 1)
            throw  " Target application missing\n";
}

void Sperf::config_output(string path)
{
    program_name= args[1];
    if(config_file == "")
        config_file= program_name;
    result_file = get_perfpath(path, RESULT_PATH);

    if(opendir("/result") == NULL)
        mkdir("../results/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    else
        throw  " Failed to create the result folder: %s\n";

    if(out_csv)
    {
        result_file+=csv_file;
        out.open(result_file+".csv");
        out.precision(5);
        out << "\n,";
        for(uint i=0; i<list_of_threads_value.size(); i++)
            out << list_of_threads_value[i] << ",";
        out.close();

        for (uint i = 0; i < info_thr_proc[1].info.size(); i++)
        {
            out.open(result_file+"_parallel_region_"+intToString(i+1)+".csv");
            out.precision(5);
            out << "\n,";
            for(uint i=0; i<list_of_threads_value.size(); i++)
                out << list_of_threads_value[i] << ",";
            out.close();
        }
    }
    else
    {
        time_t rawtime = time(NULL);
        struct tm *local = localtime(&rawtime);
        stringstream ss;
        ss << local->tm_mday << "-" << local->tm_mon + 1 << "-" << local->tm_year + 1900
           << "-" << local->tm_hour << "h-" << local->tm_min << "m-" << local->tm_sec << "s.txt";
        result_file+=ss.str();
    }
}

// read the config file
void Sperf::read_config_file(string exec_path)
{
    printf(BLUE "[Sperf]" RESET " Reading sperf_exec.conf\n");

    ifstream conf_file;
    string step_type;
    string str;
    string config_path;

    int step=1, max_threads;
    bool flag_list= 0, flag_max_threads = 0, flag_step_type = 0, flag_step_value = 0, flag_num_tests = 0;

    // get the confige file path
    config_path = get_perfpath(exec_path, ETC_PATH);

    conf_file.open(config_path);
    if(!conf_file)
        throw  " Failed to open configuration file "+config_path+"\n";

    while(!conf_file.eof())
    {
        conf_file >> str;
        if(conf_file.eof())
            break;
        if(str[0] == '#')
        {
            getline(conf_file, str);
            continue;
        }
        else
        {
            if(str.substr(0,16) == "number_of_tests=")
            {
                printf(BLUE "[Sperf]" RESET " Retrieving number of testes...\n");
                if (str.size() == 16)
                    throw  " You must specify a number of tests\n";
                else
                {
                    str= str.substr(16,str.size());
                    num_exec= stringToInt(str);
                    flag_num_tests = 1;
                }
            }
            else if(str.substr(0,20) == "list_threads_values=")
            {
                if(str.size() == 20 || str == "list_threads_values={}")
                    flag_list = 0;
                else
                {
                    printf(BLUE "[Sperf]" RESET " Retrieving the list of threads values...\n");
                    str= str.substr(20,str.size());
                    if(str[str.size()-1] != '}' || str[0] != '{')
                        throw  " Format error on list_threads_values\n";
                    stringstream ss(str.substr(1,str.size()-2));
                    while(getline(ss, str, ','))
                        list_of_threads_value.push_back(stringToInt(str));
                    flag_list = 1;
                }
            }
            else if(str.substr(0,19) == "max_number_threads=")
            {
                if (flag_list != 1)
                {
                    if (str.size() == 19)
                        throw  " You must specify a maximum number of threads\n";
                    else
                    {
                        str= str.substr(19,str.size());
                        max_threads= stringToInt(str);
                        flag_max_threads = 1;
                    }
                }
            }
            else if(str.substr(0,13) == "type_of_step=")
            {
                if (flag_list != 1)
                {
                    if(str.size() == 13)
                        throw  " You must specify a step method to increment the number of threads\n";
                    else
                    {
                        str= str.substr(13,str.size());
                        step_type= str;
                        if((step_type != "constant" && step_type != "power") || (step_type.size()!=8 && step_type.size()!=5))
                        {
                            fputs( " Invalid step method\n", stderr);
                            exit(1);
                        }
                        flag_step_type = 1;
                    }
                }
            }
            else if(str.substr(0,14) == "value_of_step=")
            {
                if (flag_list != 1)
                {
                    if (str.size() == 14)
                        throw  " You must specify a step value to increment the number of threads\n";
                    else
                    {
                        str= str.substr(14,str.size());
                        step= stringToInt(str);
                        flag_step_value = 1;
                    }
                }
            }
            else if(str.substr(0,13) == "list_of_args=")
            {
                printf(BLUE "[Sperf]" RESET " Retrieving the list of arguments...\n");
                char c;
                str= str.substr(13,str.size());
                conf_file.seekg(-str.size(), conf_file.cur);
                str= "";
                conf_file >> c;
                str+=c;
                while(conf_file >> noskipws >> c)
                {
                    if(c == '\n')
                        continue;
                    str+=c;
                }
                if(str.size() == 13 || str == "list_of_args={}"
                || str[str.size()-1] != '}' || str[0] != '{')
                    throw  " Format error on list_of_args\n";
                else
                {
                    stringstream ss1(str.substr(1, str.size()-2));
                    while(getline(ss1, str, ','))
                    {
                        char** narg= (char**)malloc(2*sizeof(char*));
                        narg[0]= (char*)malloc(250*sizeof(char));
                        strcpy(narg[0], program_name.c_str());
                        int cont= 1;
                        stringstream ss2(str);
                        while(getline(ss2, str, ' '))
                        {
                            narg[cont]= (char*)malloc(250*sizeof(char));
                            strcpy(narg[cont], str.c_str());
                            cont++;
                            narg= (char**)realloc(narg, (cont+1)*sizeof(char*));
                        }
                        narg[cont]= (char)0;
                        list_of_args.push_back(narg);
                        list_of_args_num.push_back(cont);
                    }
                }
            }
            else
            {
                throw  " Invalid configuration variable\n";
            }
        }
    }
    // verify correctly config file
    if (flag_num_tests == 0)
        throw  " 'number_of_tests' variable missing\n";
    if (flag_list == 0 && (flag_max_threads == 0 || flag_step_value == 0 || flag_step_type== 0))
        throw  " The number of threads to be executed must be set properly.\n" \
         " Define 'list_values_threads' variable or the set of three variables 'max_number_threads', 'type_of_step' and 'value_of_step'\n";
    if (flag_list == 0)
    {
        printf(BLUE "[Sperf]" RESET " Retrieving the list of threads values...\n");
        // calculate the steps
        if(step_type == "constant")
        {
            list_of_threads_value.push_back(1);
            for(int k=1+step; k<=max_threads; k+=step)
                list_of_threads_value.push_back(k);
        }
        else if(step_type == "power")
        {
            list_of_threads_value.push_back(1);
            for(int k=step; k<=max_threads; k*=step)
                list_of_threads_value.push_back(k);
        }
    }
    conf_file.close();
}

void Sperf::run()
{
    for(uint current_exec = 0; current_exec < num_exec; current_exec++)
    {
        printf(BLUE "[Sperf]" RESET " Current execution %d of %d\n", current_exec + 1, num_exec);
        for(uint current_arg=0; current_arg<list_of_args.size() || current_arg==0; current_arg++)
        {
            proc_info procInfo;
            procInfo.args= args;
            procInfo.cur_exec= current_exec;
            procInfo.num_args= num_args;
            procInfo.current_arg= current_arg;

            if(list_of_args.size()!=0)
            {
                printf(BLUE "[Sperf]" RESET " Current argument %d of %ld ", current_arg + 1, list_of_args.size());
                for(uint i=0; i<list_of_args_num[current_arg]; i++)
                    printf("%s ", list_of_args[current_arg][i]);
                printf("\n");
            }

            for(uint current_thr=0;  current_thr<list_of_threads_value.size(); current_thr++)
            {
                printf(BLUE "[Sperf]" RESET " Executing for %d threads\n", list_of_threads_value[current_thr]);
                pid_t pid_child;
                set_perfcfg(list_of_threads_value[current_thr], SET_THREADS);

                if (pipe(pipes) == -1)
                    throw  " IPC initialization error: %s\n";
                set_perfcfg(pipes[1], SET_PIPE);
                if ((pid_child = fork()) < 0)
                    throw  " Failed to fork(): %s\n";

                procInfo.info.clear();
                GET_TIME(procInfo.start);
                /* Se for o processo filho, argv[1] (o nome da aplicação passada por linha de comando) é executado, com os argumentos
                    especificados pelo vetor de strings args + 1 (o primeiro elemento de args é ./Sperf. Portanto, é descartado) */
                if (pid_child == 0)
                {
                    if (close(pipes[0]) == -1)
                        throw  " Failed to close() IPC: %s\n";
                    if (optset != 0)
                        sprintf(args[optset], "%d", list_of_threads_value[current_thr]);
                    if (execv(args[1], list_of_args.empty()?args+1:list_of_args[current_arg]) == -1)
                        throw  " Failed to start the target application: %s\n";
                }
                else
                {
                    if (close(pipes[1]) == -1)
                        throw  " Failed to close IPC: %s\n";

                    int last_mark= -1;
                    while (!waitpid(pid_child, 0, WNOHANG))
                    {
                        s_info info;
                        if ((int) read(pipes[0], &info, sizeof(s_info)) == -1)
                            throw  " Reading from the pipe has failed: %s\n";
                        if(last_mark != info.s_mark)
                        {
                            procInfo.info[info.s_mark]= info;
                            last_mark= info.s_mark;
                        }
                        else if(info.s_time > procInfo.info[info.s_mark].s_time)
                        {
                            procInfo.info[info.s_mark]= info;
                        }
                    }
                    //time_singleThrPrl.resize(time_info.size());
                    GET_TIME(procInfo.end);
                    info_thr_proc[list_of_threads_value[current_thr]]= procInfo;

                    if (close(pipes[0]) == -1)
                        throw  " Failed to close IPC: %s\n";
                }
            }
            if(out_csv)
                store_time_information_csv();
            else
                store_time_information();
        }
    }
    for(uint i=0; i<num_args; i++)
        free(args[i]);
    free(args);

    for(uint i=0; i<list_of_args.size(); i++)
    {
        for(uint j=0; j<list_of_args_num[i]; j++)
            free(list_of_args[i][j]);
        free(list_of_args[i]);
    }
}

void Sperf::store_time_information()
{
    ///TODO can broke, use interator
    out.open(result_file, ios::app);
    out << "\n-----> Execution number " << info_thr_proc[1].cur_exec + 1 << " for " << info_thr_proc[1].args[1]
    << " and " << info_thr_proc[1].current_arg << " argument" << ":\n";

    for(auto cur_thrs : list_of_threads_value)
    {
        float time_singleThr_total= info_thr_proc[1].end-info_thr_proc[1].start;

        out << "\n\t--> Result for "<< cur_thrs << " threads, application " << info_thr_proc[1].args[1] << ", arguments: ";
        for(uint i = 2; i < info_thr_proc[1].num_args; i++)
        {
            if (i != optset)
                out <<  info_thr_proc[1].args[i] << " ";
        }
        if(!list_of_args_num.empty())
        for(uint i=0; i<list_of_args_num[info_thr_proc[1].current_arg]; i++)
            out << list_of_args[info_thr_proc[1].current_arg][i] << " ";
        out << "\n";
        for (uint i=0; i<info_thr_proc[cur_thrs].info.size(); i++)
        {
            out << "\n\t\t Parallel execution time of the region " << i+1
                << ", lines " << info_thr_proc[cur_thrs].info[i].s_start_line << " to " << info_thr_proc[cur_thrs].info[i].s_stop_line
                << " on file " << info_thr_proc[cur_thrs].info[i].s_filename << " : " << info_thr_proc[cur_thrs].info[i].s_time << "seconds\n";
            out << "\t\t Speedup for the parallel region " << i+1 << " : "
                << info_thr_proc[1].info[i].s_time/info_thr_proc[cur_thrs].info[i].s_time << "\n";
        }
        out << "\n\t\t Total time of execution: "
            << info_thr_proc[cur_thrs].end-info_thr_proc[cur_thrs].start << " seconds\n";
        out << "\t\t Speedup for the entire application: "
            << time_singleThr_total/(float)(info_thr_proc[cur_thrs].end-info_thr_proc[cur_thrs].start) << "\n";
    }
    out.close();
}
void Sperf::store_time_information_csv()
{
    ///TODO can broke, use interator
    out.open(result_file+".csv", ios::app);
    out << "\n" << info_thr_proc[1].current_arg+1 << ",";
    out.close();
    for(uint i=0; i<info_thr_proc[1].info.size(); i++)
    {
        out.open(result_file+"_parallel_region_"+intToString(i+1)+".csv", ios::app);
        out << "\n" << info_thr_proc[1].current_arg+1 << ",";
        out.close();
    }
    for(auto cur_thrs : list_of_threads_value)
    {
        float time_singleThr_total= info_thr_proc[1].end-info_thr_proc[1].start;
        out.open(result_file+".csv", ios::app);
        out << fixed << time_singleThr_total/(float)(info_thr_proc[cur_thrs].end-info_thr_proc[cur_thrs].start) << ",";
        out.close();

        for(uint i=0; i<info_thr_proc[cur_thrs].info.size(); i++)
        {
            out.open(result_file+"_parallel_region_"+intToString(i+1)+".csv", ios::app);
            out << info_thr_proc[1].info[i].s_time/info_thr_proc[cur_thrs].info[i].s_time << ",";
            out.close();
        }
    }
    static uint last_exec=0;
    if(last_exec!=info_thr_proc[1].cur_exec)
    {
        out.open(result_file+".csv", ios::app);
        last_exec= info_thr_proc[1].cur_exec;
        out << "\n,";
        out.close();
        for (uint i = 0; i < info_thr_proc[1].info.size(); i++)
        {
            out.open(result_file+"_parallel_region_"+intToString(i+1)+".csv", ios::app);
            out << "\n,";
            out.close();
        }
    }
}
