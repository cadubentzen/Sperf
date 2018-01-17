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

#include <cstdlib>
#include <cstring>

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

#include "../include/sperfops.h"
#include "../include/sperf_instr.h"


class Sperf
{
private:
    enum PerfConfig { SET_THREADS, SET_PIPE };
    enum PathConfig { ETC_PATH, RESULT_PATH };
    enum OutPutType { XML, CSV, JSON };
public:
    Sperf(char* argv[], int argc);
    void run();
private:
    string get_perfpath(const string& argmnt, Sperf::PathConfig op);
    void set_perfcfg(int val, Sperf::PerfConfig op);

    void config_menu(char* argv[], int argc);
    void config_output(const string& path);
    void read_config_file(const string& exec_path);

    void store_time_information(int current_arg, int cur_exec);
    void store_time_information_csv(int current_arg, int cur_exec);
    void store_time_information_xml(int current_arg, int cur_exec);
    void store_time_information_json(int current_arg, int cur_exec);
private:
    struct proc_info
    {
        double start, end;
        map<int, s_info> info;
    };
    map<int, proc_info> map_thr_info;
    int pipes[2];/* pipes[0] = leitura; pipes[1] = escrita */
    int optset, num_exec, num_args;
    OutPutType out_filetype;
    string config_file, out_filename, program_name, result_file;
    vector<int> list_of_threads_value, list_of_args_num;
    vector<char**> list_of_args;
    char** args;
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
    catch(string& e)
    {
        cerr << RED "[Sperf]" RESET << " " << e << endl;
    }
    return 0;
}

Sperf::Sperf(char* argv[], int argc)
{
    optset=num_exec=num_args= 0;
    out_filetype= OutPutType::JSON;

    config_menu(argv, argc);

    read_config_file(argv[0]);

    config_output(argv[0]);
}

// set environment variables to control number of threads
// or set pipe
void Sperf::set_perfcfg(int val, Sperf::PerfConfig op)
{
    string str= to_string(val);
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
string Sperf::get_perfpath(const string& argmnt, Sperf::PathConfig op)
{
    string path;
    path=argmnt.substr(0,argmnt.find_last_of('/'));

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
    args = (char **) malloc((argc + 1)*sizeof(char*));
    do
    {
        if(string(argv[i]) == "-t")
        {
            i++;
            if(i<argc) // cheking if the user pass the argument
            {
                if(string(argv[i]) == "csv")
                    out_filetype= OutPutType::CSV;
                else if(string(argv[i]) == "xml")
                    out_filetype= OutPutType::XML;
                else if(string(argv[i]) == "json")
                    out_filetype= OutPutType::JSON;
                else
                {
                    throw RED" Filetype not supported";
                }
            }
        }
        if(string(argv[i]) == "-c") // passing the config file path
        {
            i++;
            if(i<argc) // cheking if the user pass the argument
                config_file= argv[i];
        }
        else if(string(argv[i]) == "-o")
        {
            i++;
            if(i<argc) // cheking if the user pass the argument
                out_filename= argv[i];
        }
        else // other arguments
        {
            args[num_args] = (char *)malloc(250*sizeof(char));
            strcpy(args[num_args], argv[i]);
            num_args++;
        }
        i++;
    } while (i < argc);

    args[num_args] = nullptr;

    if(num_args == 1)
            throw  "Target application missing\n";
}


// read the config file
void Sperf::read_config_file(const string& exec_path)
{
    ifstream conf_file;
    string step_type;
    string str;
    string config_path;

    int step=1, max_threads= 0;
    bool flag_list= false, flag_max_threads = false, flag_step_type = false, flag_step_value = false, flag_num_tests = false;

    // get the confige file path
    program_name= args[1];
    if(config_file.empty())
        config_file= program_name.substr(program_name.find_last_of('/')+1,program_name.size());
    config_path= get_perfpath(exec_path, ETC_PATH)+config_file+".conf";

    cout << BLUE "[Sperf]" RESET " Reading " << config_path << endl;
    conf_file.open(config_path.c_str());
    if(!conf_file)
    {
        //throw  " Failed to open configuration file "+config_path+"\n";
        cout << BLUE "[Sperf]" RESET " Creating cofiguration file..." << endl;
        if(opendir("../etc/") == nullptr)
        {
            mkdir("../etc/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            if(opendir("../etc/") == nullptr)
                throw  " Failed to create the etc folder: \n";
        }
        string path_to_conf= exec_path.substr(0,exec_path.find_last_of('/'))+"/../etc/sperf_default_exec.conf";
        ifstream default_file(path_to_conf.c_str());
        if(!default_file)
            throw RED "Erro on finding sperf_default_exec on etc dir";
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
                    num_exec= stoi(str);
                    flag_num_tests = true;
                }
            }
            else if(str.substr(0,20) == "list_threads_values=")
            {
                if(str.size() == 20 || str == "list_threads_values={}")
                    flag_list = false;
                else
                {
                    cout << BLUE "[Sperf]" RESET " Retrieving the list of threads values..." << endl;
                    str= str.substr(20,str.size());
                    if(str[str.size()-1] != '}' || str[0] != '{')
                        throw  "Format error on list_threads_values\n";
                    stringstream ss(str.substr(1,str.size()-2));
                    while(getline(ss, str, ','))
                        list_of_threads_value.push_back(stoi(str));
                    flag_list = true;
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
                        max_threads= stoi(str);
                        flag_max_threads = true;
                    }
                }
            }
            else if(str.substr(0,13) == "type_of_step=")
            {
                if(!flag_list)
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
                        flag_step_type = true;
                    }
                }
            }
            else if(str.substr(0,14) == "value_of_step=")
            {
                if(!flag_list)
                {
                    if(str.size() == 14)
                        throw  "You must specify a step value to increment the number of threads\n";
                    else
                    {
                        str= str.substr(14,str.size());
                        step= stoi(str);
                        flag_step_value = true;
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
                        narg[cont]= nullptr;
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
    if (!flag_num_tests)
        throw  "'number_of_tests' variable missing\n";
    if (!flag_list && (!flag_max_threads || !flag_step_value || !flag_step_type))
        throw  "The number of threads to be executed must be set properly.\n" \
         " Define 'list_values_threads' variable or the set of three variables 'max_number_threads', 'type_of_step' and 'value_of_step'\n";
    if (!flag_list)
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

void Sperf::config_output(const string& path)
{
    if(opendir("../results/") == nullptr)
    {
        mkdir("../results/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
        if(opendir("../results/") == nullptr)
            throw  " Failed to create the result folder: \n";
    }
    result_file= get_perfpath(path, RESULT_PATH);
    if(!out_filename.empty())
        result_file+= out_filename;
    else
    {
        time_t rawtime = time(nullptr);
        struct tm *local = localtime(&rawtime);
        stringstream ss;
        ss << local->tm_mday << "-" << local->tm_mon + 1 << "-" << local->tm_year + 1900
           << "-" << local->tm_hour << "h-" << local->tm_min << "m-" << local->tm_sec << "s";
        result_file+=program_name+"_out_";
//        result_file+=ss.str();
    }
}


void Sperf::run()
{
    cout << BLUE "[Sperf]" RESET " Copyright (C) 2017" << endl;
    for(int current_exec = 0; current_exec < num_exec; current_exec++)
    {
        cout << BLUE "[Sperf]" RESET " Current execution " << current_exec+1 << " of " << num_exec << endl;
        for(int current_arg=0; current_arg<list_of_args.size() || current_arg==0; current_arg++)
        {
            proc_info procInfo;
            if(!list_of_args.empty())
            {
                for(int i=0; i<list_of_args_num[current_arg]; i++)
                    if(string(list_of_args[current_arg][i]) == "nt")
                        optset= i;
            }

            for(int current_thr=0;  current_thr<list_of_threads_value.size(); current_thr++)
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
                        throw  "Failed to close() IPC: %s\n";
                    if (optset != 0)
					{
						if(list_of_args.empty())
						{
                            if(args[optset+1] == nullptr){
                                throw "Argument not exist\n";
							}
	                        sprintf(args[optset+1], "%d", list_of_threads_value[current_thr]);
						}
						else
						{
                            if(list_of_args[current_arg][optset] == nullptr)
                                throw "Argument not exist\n";
							sprintf(list_of_args[current_arg][optset], "%d", list_of_threads_value[current_thr]);
                            cout << BLUE "[Sperf]" RESET " Current argument " << current_arg + 1 << " of " << list_of_args.size() << endl;
                            for(int i=0; i<list_of_args_num[current_arg]; i++)
                                cout << list_of_args[current_arg][i] << " ";
                            cout << endl;
						}
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

                    if (close(pipes[0]) == -1)
                        throw  "Failed to close IPC: %s\n";
                }
            }
            if(optset != 0)
                sprintf(list_of_args[current_arg][optset], "%s", "nt");
            cout << BLUE "[Sperf]" RESET "Saving to the file" << endl;
            if(out_filetype == OutPutType::CSV)
                store_time_information_csv(current_arg, current_exec);
            if(out_filetype == OutPutType::XML)
                store_time_information_xml(current_arg, current_exec);
            if(out_filetype == OutPutType::JSON)
                store_time_information_json(current_arg, current_exec);
        }
    }

    for(int i=0; i<num_args; i++)
        free(args[i]);
    free(args);

    for(int i=0; i<list_of_args.size(); i++)
    {
        for(int j=0; j<list_of_args_num[i]; j++)
            free(list_of_args[i][j]);
        free(list_of_args[i]);
    }
}

void Sperf::store_time_information(int current_arg, int cur_exec)
{
    out.open(result_file.c_str(), ios::app);
    out << "\n-----> Execution number " << cur_exec + 1 << " for " << program_name
    << " and " << current_arg << " argument" << ":\n";

    for(int j=0; j<list_of_threads_value.size(); j++)
    {
        int cur_thrs= list_of_threads_value[j];
        double time_singleThr_total= map_thr_info[1].end-map_thr_info[1].start;
        out << "\n\t--> Result for "<< cur_thrs << " threads, application " << program_name << ", arguments: ";
        for(int i = 2; i<num_args; i++)
        {
            if (i != optset)
                out << args[i] << " ";
        }
        if(!list_of_args_num.empty())
            for(int i=0; i<list_of_args_num[current_arg]; i++)
                out << list_of_args[current_arg][i] << " ";
        out << "\n";

        for(auto it : map_thr_info[cur_thrs].info)
        {
            out << "\n\t\t Parallel execution time of the region " << it.first+1
                << ", lines " << it.second.s_start_line << " to " << it.second.s_stop_line
                << " on file " << it.second.s_filename << " : " << it.second.s_time << "seconds\n";
            out << "\t\t Speedup for the parallel region " << it.first+1 << " : "
                << map_thr_info[1].info[it.first].s_time/it.second.s_time << "\n";
        }

        out << "\n\t\t Total time of execution: "
            << map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start << " seconds\n";
        out << "\t\t Speedup for the entire application: "
            << time_singleThr_total/(float)(map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start) << "\n";
    }
    out.close();
}

/// MELHORAR
void Sperf::store_time_information_csv(int current_arg, int cur_exec)
{
    static bool ft= false;
    if(!ft)
    {
        out.open(string(result_file+".csv").c_str());
        out.precision(5);
        out << "\n,";
        for(int i=0; i<list_of_threads_value.size(); i++)
            out << list_of_threads_value[i] << ",";
        out.close();
        for(auto it : map_thr_info[1].info)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".csv").c_str(), ios::app);
            out.precision(5);
            out << "\n,";
            for(int i=0; i<list_of_threads_value.size(); i++)
                out << list_of_threads_value[i] << ",";
            out.close();
        }
        ft= true;
    }
    static int last_exec=-1;
    if(last_exec != cur_exec)
    {
        out.open(string(result_file+".csv").c_str(), ios::app);
        last_exec= cur_exec;
        out << "\n,";
        out.close();

        for(auto it : map_thr_info[1].info)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".csv").c_str(), ios::app);
            out << "\n,";
            out.close();
        }
    }

    out.open(string(result_file+".csv").c_str(), ios::app);
    out << "\n" << current_arg+1 << ",";
    out.close();

    for(auto it : map_thr_info[1].info)
    {
        out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".csv").c_str(),ios::app);
        out << "\n" << current_arg+1 << ",";
        out.close();
    }

    for(int j=0; j<list_of_threads_value.size(); j++)
    {
        int cur_thrs= list_of_threads_value[j];
        double time_singleThr_total= map_thr_info[1].end-map_thr_info[1].start;
        out.open(string(result_file+".csv").c_str(), ios::app);
        out << fixed << time_singleThr_total/(float)(map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start)/cur_thrs << ",";
        out.close();

        //map<int, s_info>::iterator
        for(auto it : map_thr_info[cur_thrs].info)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".csv").c_str(), ios::app);
            out << map_thr_info[1].info[it.first].s_time/it.second.s_time/cur_thrs << ",";
            out.close();
        }
    }
}

void Sperf::store_time_information_xml(int current_arg, int cur_exec)
{
    static bool ft= false;
    int ini_th= list_of_threads_value.empty()?1:list_of_threads_value[0];
    if(!ft)
    {
        out.open(string(result_file+".xml").c_str(), ios::app);
        out << "<regiao>" << endl
                    << "\t<l>" << map_thr_info[ini_th].info.begin()->second.s_start_line << "</l>" << endl
                    << "\t<l>" << map_thr_info[ini_th].info.begin()->second.s_stop_line  << "</l>" << endl
					<< "\t<p>" << program_name << "</p>" << endl;
        out.close();
        for(auto it : map_thr_info[ini_th].info)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".xml").c_str(), ios::app);
            out.precision(5);
            out << "<regiao>" << endl
                    << "\t<l>" << it.second.s_start_line << "</l>" << endl
                    << "\t<l>" << it.second.s_stop_line  << "</l>" << endl
					<< "\t<p>" << it.second.s_filename  << "</p>" << endl;
            out.close();
        }
        ft= true;
    }
    static int last_exec=-1;
    if(last_exec != cur_exec)
    {
        out.open(string(result_file+".xml").c_str(), ios::app);
        last_exec= cur_exec;

        if(cur_exec != 0)
            out << "</execucoes>" << endl;
        out << endl << "<execucoes>" << endl;

        out.close();

        for(auto it : map_thr_info[ini_th].info)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".xml").c_str(), ios::app);
            if(cur_exec != 0)
                out << "</execucoes>" << endl;
            out << endl << "<execucoes>" << endl;
            out.close();
        }
    }

    out.open(string(result_file+".xml").c_str(), ios::app);
    out << "\t<item>" << endl;
    out << "\t\t<arg>" << " ";
    if(!list_of_args.empty())
        for(int i=0; i<list_of_args_num[current_arg]; i++)
            out << list_of_args[current_arg][i] << " ";
    out << "</arg>" << endl;
    out.close();

    for(auto it : map_thr_info[ini_th].info)
    {
        out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".xml").c_str(),ios::app);
        out << "\t<item>" << endl;
        out << "\t\t<arg>" << " ";
        if(!list_of_args.empty())
            for(int i=0; i<list_of_args_num[current_arg]; i++)
                out << list_of_args[current_arg][i] << " ";
        out << "</arg>" << endl;
        out.close();
    }

    for(int j=0; j<list_of_threads_value.size(); j++)
    {
        int cur_thrs= list_of_threads_value[j];
        double time_singleThr_total= map_thr_info[ini_th].end-map_thr_info[ini_th].start;
        out.open(string(result_file+".xml").c_str(), ios::app);
        out << fixed << "\t\t<execucao>" << endl;
        out << fixed << "\t\t\t<n>" << cur_thrs << "</n>" << endl;
        out << fixed << "\t\t\t<t>" << (map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start)<< "</t>" << endl;
        out << fixed << "\t\t\t<s>" << time_singleThr_total/(float)(map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start) << "</s>" << endl;
        out << fixed << "\t\t\t<e>" << time_singleThr_total/(float)(map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start)/cur_thrs << "</e>" << endl;
        out << fixed << "\t\t</execucao>" << endl;
        out.close();

        for(auto it : map_thr_info[cur_thrs].info)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".xml").c_str(), ios::app);

            out << fixed << "\t\t<execucao>" << endl;
            out << fixed << "\t\t\t<n>" << cur_thrs << "</n>" << endl;
            out << "\t\t\t<t>" << it.second.s_time<< "</t>" << endl;
            out << "\t\t\t<s>" << map_thr_info[ini_th].info[it.first].s_time/it.second.s_time<< "</s>" << endl;
            out << "\t\t\t<e>" << map_thr_info[ini_th].info[it.first].s_time/it.second.s_time/cur_thrs << "</e>" << endl;
            out << fixed << "\t\t</execucao>" << endl;

            out.close();
        }
    }

    out.open(string(result_file+".xml").c_str(), ios::app);
    out << endl << "\t</item>" << endl;
    out.close();

    for(auto it : map_thr_info[ini_th].info)
    {
        out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".xml").c_str(),ios::app);
        out << endl << "\t</item>" << endl;
        out.close();
    }

    if(cur_exec == num_exec-1 && (current_arg == list_of_args.size()-1 || list_of_args.empty()) )
    {
        out.open(string(result_file+".xml").c_str(), ios::app);
        out << "</execucoes>" << endl;
        out << "</regiao>" << endl;
        out.close();
        for(auto it : map_thr_info[ini_th].info)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".xml").c_str(), ios::app);
            out.precision(5);
            out << "</execucoes>" << endl;
            out << "</regiao>";
            out.close();
        }
    }
}

void Sperf::store_time_information_json(int current_arg, int cur_exec)
{
    static bool ft= false;
    int ini_th= list_of_threads_value.empty()?1:list_of_threads_value[0];
    if(!ft)
    {
        out.open(string(result_file+".json").c_str(), ios::app);
        out << "{" << endl
                    << "\t\"Region\":\"" << map_thr_info[ini_th].info.begin()->second.s_start_line << ", "
                    << map_thr_info[ini_th].info.begin()->second.s_stop_line  << "\"," << endl
					<< "\t\"Filename\":\"" << program_name << "\"," << endl;
                    //<< "</regiao>" << endl;
        out.close();
        for(auto it : map_thr_info[ini_th].info)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".json").c_str(), ios::app);
            out.precision(5);
            out << "{" << endl
                    << "\t\"Region\":\"" << it.second.s_start_line << ", "
                    << it.second.s_stop_line  << "\"," << endl
					<< "\t\"Filename\":\"" << it.second.s_filename  << "\"," << endl;
                    //<< "</regiao>" << endl;
            out.close();
        }
        ft= true;
    }
    static int last_exec=-1;
    if(last_exec != cur_exec)
    {
        out.open(string(result_file+".json").c_str(), ios::app);
        last_exec= cur_exec;

        if(cur_exec != 0)
            out << "\t}," << endl;
        out << "\t\"Execution " <<  cur_exec+1 << "\":{" << endl;

        out.close();

        for(auto it : map_thr_info[ini_th].info)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".json").c_str(), ios::app);
            if(cur_exec != 0)
                out << "\t}," << endl;
            out << "\t\"Execution " << cur_exec+1 << "\":{" << endl;
            out.close();
        }
    }

    out.open(string(result_file+".json").c_str(), ios::app);
    out << "\t\t\"InputData " << current_arg+1 <<  "\":{" << endl;
    out << "\t\t\t\"arguments\":\"" << " ";
    if(!list_of_args.empty())
    for(int i=0; i<list_of_args_num[current_arg]; i++)
                out << list_of_args[current_arg][i] << " ";
    out << "\"," << endl;
    out.close();

    //for(auto it= map_thr_info[ini_th].info.begin(); it!=map_thr_info[ini_th].info.end(); it++)
    for(auto it : map_thr_info[ini_th].info)
    {
        out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".json").c_str(),ios::app);
        out << "\t\t\"InputData " << current_arg+1 << "\":{" << endl;
        out << "\t\t\t\"arguments\":\"" << " ";
        if(!list_of_args.empty())
        for(int i=0; i<list_of_args_num[current_arg]; i++)
            out << list_of_args[current_arg][i] << " ";
        out << "\"," << endl;
        out.close();
    }

    for(int j=0; j<list_of_threads_value.size(); j++)
    {
        int cur_thrs= list_of_threads_value[j];
        double time_singleThr_total= map_thr_info[ini_th].end-map_thr_info[ini_th].start;
        out.open(string(result_file+".json").c_str(), ios::app);
        out << fixed << "\t\t\t\"Run " << j+1 << "\":{" << endl;
        out << fixed << "\t\t\t\t\"Num threads\":\"" << cur_thrs << "\"," << endl;
        out << fixed << "\t\t\t\t\"Time\":\"" << (map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start)<< "\"," << endl;
        out << fixed << "\t\t\t\t\"Speedup\":\"" << time_singleThr_total/(float)(map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start) << "\"," << endl;
        out << fixed << "\t\t\t\t\"Efficiency\":\"" << time_singleThr_total/(float)(map_thr_info[cur_thrs].end-map_thr_info[cur_thrs].start)/cur_thrs << "\"" << endl;
        if(j+1 != list_of_threads_value.size())
            out << fixed << "\t\t\t}," << endl;
        else
            out << fixed << "\t\t\t}" << endl;
        out.close();

        //for(auto it= map_thr_info[cur_thrs].info.begin(); it!=map_thr_info[cur_thrs].info.end(); it++)
        for(auto it : map_thr_info[cur_thrs].info)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".json").c_str(), ios::app);

            out << fixed << "\t\t\t\"Run " << j+1 << "\":{" << endl;
            out << fixed << "\t\t\t\t\"Num threads\":\"" << cur_thrs << "\"," << endl;
            out << fixed << "\t\t\t\t\"Time\":\"" << it.second.s_time<< "\","  << endl;
            out << fixed << "\t\t\t\t\"Speedup\":\"" << map_thr_info[ini_th].info[it.first].s_time/it.second.s_time<< "\","  << endl;
            out << fixed << "\t\t\t\t\"Efficiency\":\"" << map_thr_info[ini_th].info[it.first].s_time/it.second.s_time/cur_thrs << "\""  << endl;
            if(j+1 != list_of_threads_value.size())
                out << fixed << "\t\t\t}," << endl;
            else
                out << fixed << "\t\t\t}" << endl;

            out.close();
        }
    }


    out.open(string(result_file+".json").c_str(), ios::app);
    if(current_arg+1 != list_of_args_num.size())
        out << endl << "\t\t}," << endl;
    else
        out << endl << "\t\t}" << endl;

    out.close();

    for(auto it : map_thr_info[ini_th].info)
    //for(auto it= map_thr_info[ini_th].info.begin(); it!=map_thr_info[ini_th].info.end(); it++)
    {
        out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".json").c_str(),ios::app);
        if(current_arg+1 != list_of_args_num.size())
            out << endl << "\t\t}," << endl;
        else
            out << endl << "\t\t}" << endl;
        out.close();
    }

    if(cur_exec == num_exec-1 && (current_arg == list_of_args.size()-1 || list_of_args.empty()) )
    {
        out.open(string(result_file+".json").c_str(), ios::app);
        out << "\t}" << endl;
        out << "}" << endl;
        out.close();
        for(auto it : map_thr_info[ini_th].info)
        //for(auto it= map_thr_info[ini_th].info.begin(); it!=map_thr_info[ini_th].info.end(); it++)
        {
            out.open(string(result_file+"_parallel_region_"+to_string(it.first+1)+".json").c_str(), ios::app);
            out.precision(5);
            out << "\t}" << endl;
            out << "}";
            out.close();
        }
    }
}