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
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stack>
#include <map>
#include <mutex>
#include <thread>

#include "../include/sperfops.h"
#include <sys/time.h>
//#ifndef _OPENMP
#include <pthread.h>
//#endif

#define RED     			"\x1b[31m"
#define RESET   			"\x1b[0m"

using std::stack;
//static double time_begin[MAX_ANNOTATIONS], time_final[MAX_ANNOTATIONS], time_total[MAX_ANNOTATIONS];
static stack<double> time_begin;
//static map<int, double> time_begin; // <start_line, time_begin>
static int fd_pipe, mark = 0;
static bool flag_conf = false;
static stack<int> start_line, stack_call;

static void setconfig()
{
	char *str_pipe;

	str_pipe = getenv("FD_PIPE");
	if(!str_pipe)
	{
        fprintf(stderr, RED "[Sperf]" RESET " Sperf not running\n");
            exit(1);
	}
	fd_pipe = atoi(str_pipe);

	flag_conf= true;
}

static void fname(char * last, const char * f)
{
	const char * delim = "/";
	char * token, * s;
	int i = 0;

	s = strdup(f);
	token = strtok(s, delim);
	while( token != NULL )
   	{
		i++;
		strcpy(last, token);
		token = strtok(NULL, delim);
  	 }
}

void sperf_thrnum(int *valor)
{
   	char *str_thr;

   	str_thr = getenv("NUM_THRS_ATUAL");
   	if (valor != (int *) NULL)
      	*valor = atoi(str_thr);
	else
		setenv("OMP_NUM_THREADS", str_thr, 1);
}

static std::mutex mtx;

void _sperf_start(int line, const char * filename)
{
    mtx.lock();
	if (!flag_conf)
		setconfig();
    if(start_line.empty() || start_line.top() != line)
    {
        double now;
        stack_call.push(1);
        GET_TIME(now);
        time_begin.push(now);
        start_line.push(line);
    }
    else
    {
        stack_call.top()= stack_call.top()+1;
    }
    mtx.unlock();
}

static std::mutex mtx2;

void _sperf_stop(int stop_line, const char * filename)
{
    mtx2.lock();
    stack_call.top()= stack_call.top()-1;
    if(stack_call.top() == 0)
    {
        stack_call.pop();
        static double time_final;
        GET_TIME(time_final);
        time_final-=time_begin.top();
        time_begin.pop();

        s_info info;

        info.s_mark = mark++;
        info.s_time = time_final;
        info.s_start_line = start_line.top();
        info.s_stop_line = stop_line;
        start_line.pop();

        char only_filename[64];
        fname(only_filename, filename);
        strcpy(info.s_filename, only_filename);

        if ((int) write(fd_pipe, &info, sizeof(s_info)) == -1)
        {
            fprintf(stderr, RED "[Sperf]" RESET " Writing to the pipe has failed: %s\n", strerror(errno));
            exit(1);
        }
    }
    mtx2.unlock();
}
//#ifndef _OPENMP
//static std::vector<pthread_t> thr_start;
static std::map<pthread_t, int> thr_line;
static std::map<int, double> line_time;

void _sperf_pthstart(pthread_t thr, int line, const char * filename)
{
	if (!flag_conf)
        setconfig();

    double now;
    GET_TIME(now);
    thr_line[thr]= line;
    line_time[line]= now;
}

void _sperf_pthstop(pthread_t thr_stop, int stop_line, const char * filename)
{
    int line= thr_line[thr_stop];
    double time= line_time[line];

    static double time_final;
    GET_TIME(time_final);
    time_final-= time;

	int l_mark = 0;

	s_info info;

	info.s_mark = l_mark;
	info.s_time = time_final;
	info.s_start_line = line;
	info.s_stop_line = stop_line;

	char only_filename[64];
	fname(only_filename, filename);
	strcpy(info.s_filename, only_filename);

	if ((int) write(fd_pipe, &info, sizeof(s_info)) == -1)
    {
        fprintf(stderr, RED "[Sperf]" RESET " Writing to the pipe has failed: %s\n", strerror(errno));
        exit(1);
    }
}
//#endif
