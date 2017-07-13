/*
    Copyright (C) 2017 Márcio Jales, Vitor Ramos

    This program is free software: you can redistribute it and/or modify
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>

using namespace std;

#include "sperfops.h"
#include "sperf_instr.h"

typedef unsigned int uint;

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

    void store_time_information(uint current_arg, uint cur_exec);
    void store_time_information_csv(uint current_arg, uint cur_exec);
    void store_time_information_xml(uint current_arg, uint cur_exec);
private:
    struct proc_info
    {
        double start, end;
        map<int, s_info> info;
    };
    map<uint, proc_info> map_thr_info;
    //map<uint, proc_info> media;
    int pipes[2];/* pipes[0] = leitura; pipes[1] = escrita */
    uint optset, num_exec, num_args;
    bool out_filename;
    char** args;
    string config_file, csv_file, program_name, result_file;
    vector<uint> list_of_threads_value, list_of_args_num;
    vector<char**> list_of_args;
    ofstream out;
};

int main(int argc, char *argv[])
{
    try
    {
        if(argc > 1 && string(argv[1]) == "-i")
        {
            Instrumentation inst;
            if(argc == 2)
                throw  "missing arguments to instrumentation";
            else if(argc == 3)
                inst.read_config_file(argv[2]);
            else
                inst.read_argments(argv, argc);
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
    optset=num_exec=num_args= 0;
    out_filename= false;

    config_menu(argv, argc);

    read_config_file(argv[0]);

    config_output(argv[0]);
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
        throw  "Invalid option in set_perfcfg";
}

// get program path
string Sperf::get_perfpath(string argmnt, Sperf::PathConfig op)
{
    string path;
    path=argmnt.substr(0,argmnt.find_last_of("/"));

    if (op == ETC_PATH)
        path+="/../etc/";
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
        else if(string(argv[i]) == "-c") // passing the config file path
        {
            i++;
            if(i<argc) // cheking if the user pass the argument
                config_file= argv[i];
        }
        else if(string(argv[i]) == "-o")
        {
            out_filename= true;
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
        cout << BLUE "[Sperf]" RESET " sperf_thrnum function required" << endl;
    else
        cout << BLUE "[Sperf]" RESET " Thread value passed by command line argument to the target application. sperf_thrnum function not required\n";

    if(num_args == 1)
            throw  "Target application missing\n";
}


// read the config file
void Sperf::read_config_file(string exec_path)
{

    ifstream conf_file;
    string step_type;
    string str;
    string config_path;

    int step=1, max_threads= 0;
    bool flag_list= 0, flag_max_threads = 0, flag_step_type = 0, flag_step_value = 0, flag_num_tests = 0;

    // get the confige file path
    program_name= args[1];
    if(config_file == "")
        config_file= program_name.substr(program_name.find_last_of("/")+1,program_name.size());
    config_path= get_perfpath(exec_path, ETC_PATH)+config_file+".conf";

    cout << BLUE "[Sperf]" RESET " Reading " << config_path << endl;
    conf_file.open(config_path.c_str());
    if(!conf_file)
    {
        //throw  " Failed to open configuration file "+config_path+"\n";
        cout << BLUE "[Sperf]" RESET " Creating cofiguration file..." << endl;
        string path_to_conf= exec_path.substr(0,exec_path.find_last_of("/"))+"/../etc/sperf_default_exec.conf";
        ifstream default_file(path_to_conf.c_str());
        ofstream new_file(config_path.c_str());
        string buffer;
        while(getline(default_file, buffer))
            new_file << buffer << endl;
        default_file.close();
        new_file.close();
        conf_file.open(config_path.c_str());
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
                cout << BLUE "[Sperf]" RESET " Retrieving number of testes..." << endl;
                if (str.size() == 16)
                    throw  "You must specify a number of tests\n";
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
                    cout << BLUE "[Sperf]" RESET " Retrieving the list of threads values..." << endl;
                    str= str.substr(20,str.size());
                    if(str[str.size()-1] != '}' || str[0] != '{')
                        throw  "Format error on list_threads_values\n";
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
                        throw "You must specify a maximum number of threads\n";
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
                            fputs("Invalid step method\n", stderr);
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
                        throw  "You must specify a step value to increment the number of threads\n";
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
                cout << BLUE "[Sperf]" RESET " Retrieving the list of arguments..." << endl;
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
                    throw  "Format error on list_of_args\n";
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
                throw  "Invalid configuration variable\n";
            }
        }
    }
    // verify correctly config file
    if (flag_num_tests == 0)
        throw  "'number_of_tests' variable missing\n";
    if (flag_list == 0 && (flag_max_threads == 0 || flag_step_value == 0 || flag_step_type== 0))
        throw  "The number of threads to be executed must be set properly.\n" \
         " Define 'list_values_threads' variable or the set of three variables 'max_number_threads', 'type_of_step' and 'value_of_step'\n";
    if (flag_list == 0)
    {
        cout << BLUE "[Sperf]" RESET " Retrieving the list of threads values..." << endl;
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

void Sperf::config_output(string path)
{
    result_file= get_perfpath(path, RESULT_PATH);
    if(opendir("../results/") == NULL)
    {
        mkdir("../results/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if(opendir("../results/") == NULL)
            throw  " Failed to create the result folder: \n";
    }
    if(out_filename)
    {
        result_file+=csv_file;
        out.open(string(result_file+".csv").c_str());
        out.precision(5);
        out << "\n,";
        for(uint i=0; i<list_of_threads_value.size(); i++)
            out << list_of_threads_value[i] << ",";
        out.close();
    }
    else
    {
        time_t rawtime = time(NULL);
        struct tm *local = localtime(&rawtime);
        stringstream ss;
        ss << local->tm_mday << "-" << local->tm_mon + 1 << "-" << local->tm_year + 1900
           << "-" << local->tm_hour << "h-" << local->tm_min << "m-" << local->tm_sec << "s";
        //result_file+=ss.str();
        result_file+=program_name+"_out";
//        out.open(string(result_file+".xml").c_str());
//        out.precision(5);
//        out << "<nucleos>";
//        for(uint i=0; i<list_of_threads_value.size(); i++)
//            out << " <n>" << list_of_threads_value[i] << "</n>";
//        out << "\n</nucleos>\n\n";
//        out.close();
    }
}


void Sperf::run()
{
    cout << BLUE "[Sperf]" RESET " Copyright (C) 2017" << endl;
    for(uint current_exec = 0; current_exec < num_exec; current_exec++)
    {
        cout << BLUE "[Sperf]" RESET " Current execution " << current_exec+1 << " of " << num_exec << endl;
        for(uint current_arg=0; current_arg<list_of_args.size() || current_arg==0; current_arg++)
        {
            proc_info procInfo;
            if(optset != 0)
                sprintf(list_of_args[current_arg][optset], "%d", list_of_threads_value[0]);
            if(list_of_args.size()!=0)
            {
                cout << BLUE "[Sperf]" RESET " Current argument " << current_arg + 1 << " of " << list_of_args.size() << endl;
                for(uint i=0; i<list_of_args_num[current_arg]; i++)
                    cout << list_of_args[current_arg][i] << " ";
                cout << endl;
            }

            for(uint current_thr=0;  current_thr<list_of_threads_value.size(); current_thr++)
            {
                cout << BLUE "[Sperf]" RESET " Executing for " <<  list_of_threads_value[current_thr] << " threads" << endl;
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
					{
						if(list_of_args.empty())
	                        sprintf(args[optset], "%d", list_of_threads_value[current_thr]);
						else
							sprintf(list_of_args[current_arg][optset], "%d", list_of_threads_value[current_thr]);
					}
                    if (execv(args[1], list_of_args.empty()?args+1:list_of_args[current_arg]) == -1)
                        throw  "Failed to start the target application\n";
                }
                else
                {
                    if (close(pipes[1]) == -1)
                        throw  "Failed to close IPC: %s\n";

                    int last_mark= -1;
                    while (!waitpid(pid_child, 0, WNOHANG))
                    {
                        s_info info;
                        if ((int) read(pipes[0], &info, sizeof(s_info)) == -1)
                            throw  "Reading from the pipe has failed: %s\n";
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
                    map_thr_info[list_of_threads_value[current_thr]]= procInfo;

                    // on the fly average curAvg = curAvg + (newNum - curAvg)/n;
//                    media[list_of_threads_value[current_thr]].end= media[list_of_threads_value[current_thr]].end
//                                +(procInfo.end-media[list_of_threads_value[current_thr]].end)/(current_exec+1.0);
//                    media[list_of_threads_value[current_thr]].start= media[list_of_threads_value[current_thr]].start
//                                +(procInfo.start-media[list_of_threads_value[current_thr]].start)/(current_exec+1.0);
//                    for(map<int, s_info>::iterator it= media[list_of_threads_value[current_thr]].info.begin();
//                    it!=media[list_of_threads_value[current_thr]].info.end(); it++)
//                        it->second.s_time= it->second.s_time+(procInfo.info[it->first].s_time-it->second.s_time)/(current_exec+1.0);

                    if (close(pipes[0]) == -1)
                        throw  "Failed to close IPC: %s\n";
                }
            }
            if(out_filename)
                store_time_information_csv(current_arg, current_exec);
            else
                store_time_information_xml(current_arg, current_exec);
        }
    }
//    map_thr_info= media;
//    if(out_filename)
//        store_time_information_csv(list_of_args.size(), num_exec);
//    else
//        store_time_information_xml(list_of_args.size(), num_exec);

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

void Sperf::store_time_information(uint current_arg, uint cur_exec)
{
    out.open(result_file.c_str(), ios::app);
    out << "\n-----> Execution number " << cur_exec + 1 << " for " << program_name
    << " and " << current_arg << " argument" << ":\n";

    for(uint j=0; j<list_of_threads_value.size(); j++)
    //for(auto cur_thrs : list_of_threads_value)
    {
        uint cur_thrs= list_of_threads_value[j];
        float time_singleThr_total= map_thr_info[1].end-map_thr_info[1].start;
        out << "\n\t--> Result for "<< cur_thrs << " threads, application " << program_name << ", arguments: ";
        for(uint i = 2; i<num_args; i++)
        {
            if (i != optset)
                out << args[i] << " ";
        }
        if(!list_of_args_num.empty())
        for(uint i=0; i<list_of_args_num[current_arg]; i++)
            out << list_of_args[current_arg][i] << " ";
        out << "\n";

        for(map<int, s_info>::iterator it= map_thr_info[cur_thrs].info.begin(); it!=map_thr_info[cur_thrs].info.end(); it++)
        //for(auto it: map_thr_info[cur_thrs].info)
        {
            out << "\n\t\t Parallel execution time of the region " << it->first+1
                << ", lines " << it->second.s_start_line << " to " << it->second.s_stop_line
                << " on file " << it->second.s_filename << " : " << it->second.s_time << "seconds\n";
            out << "\t\t Speedup for the parallel region " << it->first+1 << " : "
                << map_thr_info[1].info[it->first].s_time/it->second.s_time << "\n";
        }

        out << "\n\t\t Total time of execution: "
            << map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start << " seconds\n";
        out << "\t\t Speedup for the entire application: "
            << time_singleThr_total/(float)(map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start) << "\n";
    }
    out.close();
}

/// MELHORAR
void Sperf::store_time_information_csv(uint current_arg, uint cur_exec)
{
    static bool ft= false;
    if(!ft)
    {
        for(map<int, s_info>::iterator it= map_thr_info[1].info.begin(); it!=map_thr_info[1].info.end(); it++)
        //for(auto it: map_thr_info[1].info)
        {
            out.open(string(result_file+"_parallel_region_"+intToString(it->first+1)+".csv").c_str(), ios::app);
            out.precision(5);
            out << "\n,";
            for(uint i=0; i<list_of_threads_value.size(); i++)
                out << list_of_threads_value[i] << ",";
            out.close();
        }
        ft= true;
    }
    static uint last_exec=-1;
    if(last_exec != cur_exec)
    {
        out.open(string(result_file+".csv").c_str(), ios::app);
        last_exec= cur_exec;
        out << "\n,";
        out.close();

        for(map<int, s_info>::iterator it= map_thr_info[1].info.begin(); it!=map_thr_info[1].info.end(); it++)
        //for(auto it: map_thr_info[1].info)
        {
            out.open(string(result_file+"_parallel_region_"+intToString(it->first+1)+".csv").c_str(), ios::app);
            out << "\n,";
            out.close();
        }
    }

    out.open(string(result_file+".csv").c_str(), ios::app);
    out << "\n" << current_arg+1 << ",";
    out.close();

    for(map<int, s_info>::iterator it= map_thr_info[1].info.begin(); it!=map_thr_info[1].info.end(); it++)
    //for(auto it: map_thr_info[1].info)
    {
        out.open(string(result_file+"_parallel_region_"+intToString(it->first+1)+".csv").c_str(),ios::app);
        out << "\n" << current_arg+1 << ",";
        out.close();
    }

    for(uint j=0; j<list_of_threads_value.size(); j++)
    //for(auto cur_thrs : list_of_threads_value)
    {
        uint cur_thrs= list_of_threads_value[j];
        float time_singleThr_total= map_thr_info[1].end-map_thr_info[1].start;
        out.open(string(result_file+".csv").c_str(), ios::app);
        out << fixed << time_singleThr_total/(float)(map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start)/cur_thrs << ",";
        out.close();

        for(map<int, s_info>::iterator it= map_thr_info[cur_thrs].info.begin(); it!=map_thr_info[cur_thrs].info.end(); it++)
        //for(auto it: map_thr_info[cur_thrs].info)
        {
            out.open(string(result_file+"_parallel_region_"+intToString(it->first+1)+".csv").c_str(), ios::app);
            out << map_thr_info[1].info[it->first].s_time/it->second.s_time/cur_thrs << ",";
            out.close();
        }
    }
}

void Sperf::store_time_information_xml(uint current_arg, uint cur_exec)
{
    static bool ft= false;
    if(!ft)
    {
        out.open(string(result_file+".xml").c_str(), ios::app);
        out << "<regiao>" << endl
                    << "\t<l>" << map_thr_info[1].info.begin()->second.s_start_line << "</l>" << endl
                    << "\t<l>" << map_thr_info[1].info.begin()->second.s_stop_line  << "</l>" << endl;
                    //<< "</regiao>" << endl;
        out.close();
        for(map<int, s_info>::iterator it= map_thr_info[1].info.begin(); it!=map_thr_info[1].info.end(); it++)
        //for(auto it: map_thr_info[1].info)
        {
            out.open(string(result_file+"_parallel_region_"+intToString(it->first+1)+".xml").c_str(), ios::app);
            out.precision(5);
            out << "<regiao>" << endl
                    << "\t<l>" << it->second.s_start_line << "</l>" << endl
                    << "\t<l>" << it->second.s_stop_line  << "</l>" << endl;
                    //<< "</regiao>" << endl;
            out.close();
        }
        ft= true;
    }
    static uint last_exec=-1;
    if(last_exec != cur_exec)
    {
        out.open(string(result_file+".xml").c_str(), ios::app);
        last_exec= cur_exec;

        out << endl << "<execucoes>" << endl;
        if(cur_exec != 0)
            out << "</execucoes>" << endl;
        out.close();

        for(map<int, s_info>::iterator it= map_thr_info[1].info.begin(); it!=map_thr_info[1].info.end(); it++)
        //for(auto it: map_thr_info[1].info)
        {
            out.open(string(result_file+"_parallel_region_"+intToString(it->first+1)+".xml").c_str(), ios::app);
            out << endl << "<execucoes>" << endl;
            if(cur_exec != 0)
                out << "</execusoes>" << endl;
            out.close();
        }
    }

    out.open(string(result_file+".xml").c_str(), ios::app);
    out << "\t<item>" << endl;
    out << "\t\t<arg>" << " ";
    if(!list_of_args.empty())
    for(uint i=0; i<list_of_args_num[current_arg]; i++)
                out << list_of_args[current_arg][i] << " ";
    out << "</arg>" << endl;
    out.close();

    for(map<int, s_info>::iterator it= map_thr_info[1].info.begin(); it!=map_thr_info[1].info.end(); it++)
    //for(auto it: map_thr_info[1].info)
    {
        out.open(string(result_file+"_parallel_region_"+intToString(it->first+1)+".xml").c_str(),ios::app);
        out << "\t<item>" << endl;
        out << "\t\t<arg>" << " ";
        if(!list_of_args.empty())
        for(uint i=0; i<list_of_args_num[current_arg]; i++)
                    out << list_of_args[current_arg][i] << " ";
        out << "</arg>" << endl;
        out.close();
    }

    for(uint j=0; j<list_of_threads_value.size(); j++)
    //for(auto cur_thrs : list_of_threads_value)
    {
        uint cur_thrs= list_of_threads_value[j];
        float time_singleThr_total= map_thr_info[1].end-map_thr_info[1].start;
        out.open(string(result_file+".xml").c_str(), ios::app);
        out << fixed << "\t\t<execucao>" << endl;
        out << fixed << "\t\t\t<n>" << cur_thrs << "</n>" << endl;
        out << fixed << "\t\t\t<t>" << (map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start)<< "</t>" << endl;
        out << fixed << "\t\t\t<s>" << time_singleThr_total/(float)(map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start) << "</s>" << endl;
        out << fixed << "\t\t\t<e>" << time_singleThr_total/(float)(map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start)/cur_thrs << "</e>" << endl;
        out << fixed << "\t\t</execucao>" << endl;
        out.close();

        for(map<int, s_info>::iterator it= map_thr_info[cur_thrs].info.begin(); it!=map_thr_info[cur_thrs].info.end(); it++)
        //for(auto it: map_thr_info[cur_thrs].info)
        {
            out.open(string(result_file+"_parallel_region_"+intToString(it->first+1)+".xml").c_str(), ios::app);

            out << fixed << "\t\t<execucao>" << endl;
            out << fixed << "\t\t\t<n>" << cur_thrs << "</n>" << endl;
            out << "<t>" << it->second.s_time<< "</t>" << endl;
            out << "<s>" << map_thr_info[1].info[it->first].s_time/it->second.s_time<< "</s>" << endl;
            out << "<e>" << map_thr_info[1].info[it->first].s_time/it->second.s_time/cur_thrs << "</e>" << endl;
            out << fixed << "\t\t</execucao>" << endl;


            out.close();
        }
    }


    out.open(string(result_file+".xml").c_str(), ios::app);
    out << endl << "\t</item>" << endl;
    out.close();

    for(map<int, s_info>::iterator it= map_thr_info[1].info.begin(); it!=map_thr_info[1].info.end(); it++)
    //for(auto it: map_thr_info[1].info)
    {
        out.open(string(result_file+"_parallel_region_"+intToString(it->first+1)+".xml").c_str(),ios::app);
        out << endl << "\t</item>" << endl;
        out.close();
    }

    if(cur_exec == num_exec-1 && (current_arg == list_of_args.size()-1 || list_of_args.size() == 0) )
    {
        out.open(string(result_file+".xml").c_str(), ios::app);
        out << "</execucoes>" << endl;
        out << "</regiao>" << endl;
        out.close();
        for(map<int, s_info>::iterator it= map_thr_info[1].info.begin(); it!=map_thr_info[1].info.end(); it++)
        //for(auto it: map_thr_info[1].info)
        {
            out.open(string(result_file+"_parallel_region_"+intToString(it->first+1)+".xml").c_str(), ios::app);
            out.precision(5);
            out << "</execucoes>" << endl;
            out << "</regiao>";
                    //<< "</regiao>" << endl;
            out.close();
        }
    }
}
