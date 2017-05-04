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

using namespace std;

#include "../include/sperfops.h"

#define SET_THREADS    	1
#define SET_PIPE       		2
#define ETC_PATH			3
#define RESULT_PATH		4

#define RED     			"\x1b[31m"
#define GREEN   			"\x1b[32m"
#define YELLOW  		"\x1b[33m"
#define BLUE    			"\x1b[34m"
#define MAGENTA 		"\x1b[35m"
#define CYAN   			"\x1b[36m"
#define RESET   			"\x1b[0m"

s_info info;

vector<float> prl_times, time_singleThrPrl;
vector<int> start_line, stop_line;

int optset = 0;
int num_exec, num_args;
string config_file, csv_file, program_name, result_file;
vector<int> list_of_threads_value;
vector<char**> list_of_args;
vector<int> list_of_args_num;
vector<string> fname;
bool out_csv= 0;

string intToString(int x);
int stringToInt(string x);

void set_perfcfg(int val, int op);
string get_path(string argmnt, int location);
void exec_conf(string exec_path);
void time_information(int cur_thrs, int cur_argm, double l_end, double l_start,
                      ofstream& out, int cur_exec, char** l_args, int l_argc);
void time_information_csv(int cur_thrs, int cur_argm, double l_end, double l_start,
                      ofstream& out, int cur_exec, char** l_args, int l_argc);
void menu_opt(char* argv[], int argc, char*** args);

int main(int argc, char *argv[])
{
    char** args;
    ofstream out;

    /* Passando os argumentos da linha de comando para a variável args */
    menu_opt(argv, argc, &args);
    if (num_args == 1)
    {
        printf(RED "[Sperf]" RESET " Target application missing\n");
        exit(1);
    }
    config_file=program_name= args[1];
    exec_conf(argv[0]);
    result_file = get_path(argv[0], RESULT_PATH);

    if(out_csv)
        result_file+=csv_file;
    else
    {
        time_t rawtime = time(NULL);
        struct tm *local = localtime(&rawtime);
        stringstream ss;
        ss << local->tm_mday << "-" << local->tm_mon + 1 << "-" << local->tm_year + 1900
           << "-" << local->tm_hour << "h-" << local->tm_min << "m-" << local->tm_sec << "s.txt";
        result_file+=ss.str();
    }


    if(opendir("/result") == NULL)
    {
        mkdir("../results/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    else
    {
        fprintf(stderr, RED "[Sperf]" RESET " Failed to create the result folder: %s\n", strerror(errno));
        exit(1);
    }

    fname.resize(MAX_ANNOTATIONS);
    /* pipes[0] = leitura; pipes[1] = escrita */
    int pipes[2];
    double start, end;
    for(int current_exec = 0; current_exec < num_exec; current_exec++)
    {
        printf(BLUE "[Sperf]" RESET " Current execution %d of %d\n", current_exec + 1, num_exec);
        for(uint current_arg=0; current_arg<list_of_args.size() || current_arg==0; current_arg++)
        {
            if(list_of_args.size()!=0)
                printf(BLUE "[Sperf]" RESET " Current argument %d of %ld\n", current_arg + 1, list_of_args.size());
            for(uint num_threads=0;  num_threads<list_of_threads_value.size(); num_threads++)
            {
                printf(BLUE "[Sperf]" RESET " Executing for %d threads\n", list_of_threads_value[num_threads]);
                pid_t pid_child;
                set_perfcfg(list_of_threads_value[num_threads], SET_THREADS);
                if (pipe(pipes) == -1)
                {
                    fprintf(stderr, RED "[Sperf]" RESET " IPC initialization error: %s\n", strerror(errno));
                    exit(1);
                }
                set_perfcfg(pipes[1], SET_PIPE);
                if ((pid_child = fork()) < 0)
                {
                    fprintf(stderr, RED "[Sperf]" RESET " Failed to fork(): %s\n", strerror(errno));
                    exit(1);
                }
                GET_TIME(start);

                /* Se for o processo filho, argv[1] (o nome da aplicação passada por linha de comando) é executado, com os argumentos
                    especificados pelo vetor de strings args + 1 (o primeiro elemento de args é ./Sperf. Portanto, é descartado) */
                if (pid_child == 0)
                {
                    if (close(pipes[0]) == -1)
                    {
                        fprintf(stderr, RED "[Sperf]" RESET " Failed to close() IPC: %s\n", strerror(errno));
                        exit(1);
                    }
                    if (optset != 0)
                        sprintf(args[optset], "%d", list_of_threads_value[num_threads]);
                    if (execv(args[1], list_of_args.empty()?args+1:list_of_args[current_arg]) == -1)
                    {
                        fprintf(stderr, RED "[Sperf]" RESET " Failed to start the target application: %s\n", strerror(errno));
                        exit(1);
                    }
                }
                else
                {
                    if (close(pipes[1]) == -1)
                    {
                        fprintf(stderr, RED "[Sperf]" RESET " Failed to close IPC: %s\n", strerror(errno));
                        exit(1);
                    }
                    prl_times.clear();
                    start_line.clear();
                    stop_line.clear();
                    fname.clear();
                    int last_mark= -1;
                    while (!waitpid(pid_child, 0, WNOHANG))
                    {
                        if ((int) read(pipes[0], &info, sizeof(s_info)) == -1)
                        {
                            fprintf(stderr, RED "[Sperf]" RESET " Reading from the pipe has failed: %s\n", strerror(errno));
                            exit(1);
                        }

                        int mark = info.s_mark;
                        if(last_mark != mark)
                        {
                            prl_times.push_back(info.s_time);
                            start_line.push_back(info.s_start_line);
                            stop_line.push_back(info.s_stop_line);
                            fname.push_back(info.s_filename);
                            last_mark= mark;
                            //cout << "Recebi : " << start_line.back() << " " << stop_line.back() << " " << mark << endl;
                        }

                    }
                    time_singleThrPrl.resize(prl_times.size());
                    GET_TIME(end);

                    if(out_csv)
                        time_information_csv(list_of_threads_value[num_threads], current_arg,
                                             end, start, out, current_exec, args, num_args);
                    else
                        time_information(list_of_threads_value[num_threads], current_arg,
                                         end, start, out, current_exec, args, num_args);

                    if (close(pipes[0]) == -1)
                    {
                        fprintf(stderr, RED "[Sperf]" RESET " Failed to close IPC: %s\n", strerror(errno));
                        exit(1);
                    }
                }
            }
        }
    }
    for(int i=0; i<num_args; i++)
        free(args[i]);
    free(args);

    for(uint i=0; i<list_of_args.size(); i++)
    {
        for(int j=0; j<list_of_args_num[i]; j++)
        {
            free(list_of_args[i][j]);
        }
        free(list_of_args[i]);
    }

    return 0;
}

string intToString(int x)
{
    stringstream ss;
    ss << x;
    return ss.str();
}
int stringToInt(string x)
{
    int n;
    stringstream ss(x);
    ss >> n;
    return n;
}

// set environment variables to control number of threads
// or set pipe
void set_perfcfg(int val, int op)
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
    {
        fputs(RED "[Sperf]" RESET " Invalid option in set_perfcfg", stderr);
        exit(1);
    }
}

// get program path
string get_path(string argmnt, int location)
{
    string path;
    path=argmnt.substr(0,argmnt.find_last_of("/"));

    if (location == ETC_PATH)
        path+="/../etc/"+string(config_file)+".conf";
    // add the result path
    if (location == RESULT_PATH)
        path+="/../results/";

    return path;
}

// read the config file
void exec_conf(string exec_path)
{
    printf(BLUE "[Sperf]" RESET " Reading sperf_exec.conf\n");

    ifstream conf_file;
    string step_type;
    string str;
    string config_path;

    int step=1, max_threads;
    bool flag_list= 0, flag_max_threads = 0, flag_step_type = 0, flag_step_value = 0, flag_num_tests = 0;

    // get the confige file path
    config_path = get_path(exec_path, ETC_PATH);

    conf_file.open(config_path);
    if(!conf_file)
    {
        fprintf(stderr, RED "[Sperf]" RESET " Failed to open %s: %s\n", config_path.c_str(), strerror(errno));
        exit(1);
    }

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
                {
                    fputs(RED "[Sperf]" RESET " You must specify a number of tests\n", stderr);
                    exit(1);
                }
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
                    {
                        fputs(RED "[Sperf]" RESET " Format error on list_threads_values\n", stderr);
                        exit(1);
                    }
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
                    {
                        fputs(RED "[Sperf]" RESET " You must specify a maximum number of threads\n", stderr);
                        exit(1);
                    }
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
                    {
                        fputs(RED "[Sperf]" RESET " You must specify a step method to increment the number of threads\n", stderr);
                        exit(1);
                    }
                    else
                    {
                        str= str.substr(13,str.size());
                        step_type= str;
                        if((step_type != "constant" && step_type != "power") || (step_type.size()!=8 && step_type.size()!=5))
                        {
                            fputs(RED "[Sperf]" RESET " Invalid step method\n", stderr);
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
                    {
                        fputs(RED "[Sperf]" RESET " You must specify a step value to increment the number of threads\n", stderr);
                        exit(1);
                    }
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
                {
                    fputs(RED "[Sperf]" RESET " Format error on list_of_args\n", stderr);
                    exit(1);
                }
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
                fputs(RED "[Sperf]" RESET " Invalid configuration variable\n", stderr);
//                exit(1);
            }
        }
    }
    // verify correctly config file
    if (flag_num_tests == 0)
    {
        fputs(RED "[Sperf]" RESET " 'number_of_tests' variable missing\n", stderr);
        exit(1);
    }
    if (flag_list == 0 && (flag_max_threads == 0 || flag_step_value == 0 || flag_step_type== 0))
    {
        fputs(RED "[Sperf]" RESET " The number of threads to be executed must be set properly.\n", stderr);
        fputs(RED "[Sperf]" RESET " Define 'list_values_threads' variable or the set of three variables 'max_number_threads', 'type_of_step' and 'value_of_step'\n", stderr);
        exit(1);
    }
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

// store time information
void time_information_csv(int cur_thrs, int cur_argm, double l_end, double l_start,
                          ofstream& out, int cur_exec, char** l_args, int l_argc)
{
    static int last_exec=0;
    static bool header= false;
    static float time_singleThrTotal;
    uint count;
    if(!header)
    {
        out.open(result_file+".csv", ios::app);
        out.precision(5);
        out << "\n,";
        for(uint i=0; i<list_of_threads_value.size(); i++)
            out << list_of_threads_value[i] << ",";
        out.close();
        for (count = 0; count < prl_times.size(); count++)
        {
            out.open(result_file+"_parallel_region_"+intToString(count+1)+".csv", ios::app);
            out.precision(5);
            out << "\n,";
            for(uint i=0; i<list_of_threads_value.size(); i++)
                out << list_of_threads_value[i] << ",";
            out.close();
        }
        header= true;
    }
    if(last_exec!=cur_exec)
    {
        out.open(result_file+".csv", ios::app);
        last_exec= cur_exec;
        out << "\n,";
        out.close();
        for (count = 0; count < prl_times.size(); count++)
        {
            out.open(result_file+"_parallel_region_"+intToString(count+1)+".csv", ios::app);
            last_exec= cur_exec;
            out << "\n,";
            out.close();
        }
    }
    if (cur_thrs == 1)
    {
        for (count = 0; count < prl_times.size(); count++)
            time_singleThrPrl[count] = prl_times[count];

        time_singleThrTotal = (float) (l_end - l_start);

        out.open(result_file+".csv", ios::app);
        out << "\n" << cur_argm+1 << ",";
        out.close();
        for (count = 0; count < prl_times.size(); count++)
        {
            out.open(result_file+"_parallel_region_"+intToString(count+1)+".csv", ios::app);
            out << "\n" << cur_argm+1 << ",";
            out.close();
        }
    }
    out.open(result_file+".csv", ios::app);
    out << fixed << time_singleThrTotal/(float)(l_end - l_start) << ",";
    out.close();
    ///TODO
    for (count = 0; count < prl_times.size(); count++)
    {
        out.open(result_file+"_parallel_region_"+intToString(count+1)+".csv", ios::app);
        out << time_singleThrPrl[count]/prl_times[count] << ",";
        out.close();
    }
}
void time_information(int cur_thrs, int cur_argm, double l_end, double l_start,
                      ofstream& out, int cur_exec, char** l_args, int l_argc)
{
    static float time_singleThrTotal;
    out.open(result_file, ios::app);
    uint count;
    if (cur_thrs == 1)
    {
        for (count = 0; count < prl_times.size(); count++)
            time_singleThrPrl[count] = prl_times[count];

        time_singleThrTotal = (float) (l_end - l_start);
        out << "\n-----> Execution number " << cur_exec + 1 << " for " << l_args[1] << " and " << cur_argm << " argument" << ":\n";
    }

    out << "\n\t--> Result for "<< cur_thrs << " threads, application " << l_args[1] << ", arguments: ";
    for(int i = 2; i < l_argc; i++)
    {
        if (i != optset)
            out <<  l_args[i] << ", ";
        if (i == l_argc - 1)
            out << "\n";
    }
    for (count = 0; count < prl_times.size(); count++)
    {
        out << "\n\t\t Parallel execution time of the region " << count+1
            << ", lines " << start_line[count] << " to " << stop_line[count] << " on file " << fname[count] << " : " << prl_times[count] << "seconds\n";
        out << "\t\t Speedup for the parallel region " << count+1 << " : " << time_singleThrPrl[count]/prl_times[count] << "\n";
    }
    out << "\n\t\t Total time of execution: " << l_end - l_start << " seconds\n";
    out << "\t\t Speedup for the entire application: " << time_singleThrTotal/(float)(l_end - l_start) << "\n";
    out.close();
}

// program configurations
void menu_opt(char* argv[], int argc, char*** args)
{
    int i=0;
    bool thrnum_req= false;
    (*args) = (char **) malloc((argc + 1)*sizeof(char*));
    do
    {
        if(string(argv[i]) == "-t") // set number of the argument that will pass the number of thread to the program
        {
            thrnum_req= true;
            i++;
            if(i<argc) // cheking if the user pass the argument
            {
                optset = stringToInt(argv[i]);
            }
        }
        else if (string(argv[i]) == "-c") // passing the config file path
        {
            i++;
            if(i<argc) // cheking if the user pass the argument
            {
                config_file= argv[i];
            }
        }
        else if(string(argv[i]) == "-o")
        {
            out_csv= true;
            i++;
            if(i<argc) // cheking if the user pass the argument
            {
                csv_file= argv[i];
            }
        }
        else // other arguments
        {
            (*args)[num_args] = (char *)malloc(250*sizeof(char));
            strcpy((*args)[num_args], argv[i]);
            num_args++;
        }
        i++;
    }
    while (i < argc);
    (*args)[num_args] = (char *) NULL;
    if(thrnum_req)
        printf(BLUE "[Sperf]" RESET " sperf_thrnum function required\n");
    else
        printf(BLUE "[Sperf]" RESET " Thread value passed by command line argument to the target application. sperf_thrnum function not required\n");
}
