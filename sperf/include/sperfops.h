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

#ifndef _SPERFOPS_H_
#define _SPERFOPS_H_ 1

#include <sys/time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define RED     			"\x1b[31m"
#define RESET   			"\x1b[0m"

#define GET_TIME(now) { \
   struct timeval t; \
   gettimeofday(&t, NULL); \
   now = t.tv_sec + t.tv_usec/1000000.0; \
}

#define	 MAX_ANNOTATIONS	64
#define  MAX_THREADS        64

struct s_info {
    int s_mark;
    float s_time;
    int s_start_line;
    int s_stop_line;
    char s_filename[64];
};

static int fd_pipe;
static bool flag_conf = false;

inline void setconfig()
{
    if(!flag_conf)
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

#ifdef _OPENMP
#include <omp.h>

#define sperf_start(id) _sperf_start(id, __LINE__, __FILE__);
#define sperf_stop(id) _sperf_stop(id, __LINE__, __FILE__);
void _sperf_start(int id, int start_line, const char * filename);
void _sperf_stop(int id, int stop_line, const char * filename);

static double id_time[MAX_ANNOTATIONS][MAX_THREADS];
static double id_start_line[MAX_ANNOTATIONS][MAX_THREADS];


void _sperf_start(int id, int start_line, const char * filename)
{
    setconfig();
    static double now;
    GET_TIME(now);
    id_time[id<0?(MAX_ANNOTATIONS+id):id][omp_get_thread_num()]= now;
    id_start_line[id<0?(MAX_ANNOTATIONS+id):id][omp_get_thread_num()]= start_line;
}

void _sperf_stop(int id, int stop_line, const char * filename)
{
    static double time_final;
    GET_TIME(time_final);
    time_final-=id_time[id][omp_get_thread_num()];

    s_info info;

    info.s_mark = id;
    info.s_time = time_final;
    info.s_start_line = id_start_line[id][omp_get_thread_num()];
    info.s_stop_line = stop_line;

    char only_filename[64];
    fname(only_filename, filename);
    strcpy(info.s_filename, only_filename);

    if (!flag_conf || (int) write(fd_pipe, &info, sizeof(s_info)) == -1)
    {
        fprintf(stderr, RED "[Sperf]" RESET " Writing to the pipe has failed: %s\n", strerror(errno));
        exit(1);
    }
}

#endif

#ifndef _OPENMP
#include <pthread.h>
#include <map>

#define sperf_start(id) _sperf_pthstart(id, __LINE__, __FILE__);
#define sperf_stop(id) _sperf_pthstop(id, __LINE__, __FILE__);
void _sperf_pthstart(int id, int start_line, const char * filename);
void _sperf_pthstop(int id, int stop_line, const char * filename);

static std::map<pthread_t, double> pid_time[MAX_ANNOTATIONS];
static std::map<pthread_t, int> pid_start_line[MAX_ANNOTATIONS];

void _sperf_pthstart(int id, int start_line, const char * filename)
{
    setconfig();
    static double now;
    GET_TIME(now);

    pid_time[id][pthread_self()]= now;
    pid_start_line[id][pthread_self()]= start_line;
}

void _sperf_pthstop(int id, int stop_line, const char * filename)
{
    int line= pid_start_line[id][pthread_self()];
    double time= pid_time[id][pthread_self()];

    static double time_final;
    GET_TIME(time_final);
    time_final-= time;

	//int l_mark = 0;

	s_info info;

	info.s_mark = id;
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

#endif

#endif
