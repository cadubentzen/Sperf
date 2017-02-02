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
#include "../include/sprofops.h"
#include <sys/time.h>
#ifndef _OPENMP
#include <pthread.h>
#endif

#define RED     			"\x1b[31m"
#define RESET   			"\x1b[0m"

static double time_begin[MAX_ANNOTATIONS], time_final[MAX_ANNOTATIONS], time_total[MAX_ANNOTATIONS];
static int fd_pipe, mark = 0, count = 0;
static int start_line[MAX_ANNOTATIONS];

static void setconfig()
{
	char *str_pipe;

	str_pipe = getenv("FD_PIPE");
	fd_pipe = atoi(str_pipe);

	count++;
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

void sprof_thrnum(int *valor)
{
   	char *str_thr;

   	str_thr = getenv("NUM_THRS_ATUAL");
   	if (valor != (int *) NULL)
      	*valor = atoi(str_thr);
	else
		setenv("OMP_NUM_THREADS", str_thr, 1);
}

void _sprof_start(int line, const char * filename)
{
	int l_mark = 0;
	static int max_mark = 0;

	if (count == 0)
		setconfig();

	while (l_mark < max_mark)
	{
		if (line != start_line[l_mark])
			l_mark++;
		else
		{
			mark = l_mark;
			GET_TIME(time_begin[mark]);
			break;
		}
	}

	if (l_mark == max_mark)
	{
		start_line[l_mark] = line;	
		mark = l_mark;
		max_mark++;
		GET_TIME(time_begin[mark]);
	}
}

void _sprof_stop(int stop_line, const char * filename)
{
	struct send_info {
		int s_mark;
		float s_time;
		int s_start_line;
		int s_stop_line;
		char s_filename[64];
	} info;

	GET_TIME(time_final[mark]);

	time_total[mark] += time_final[mark] - time_begin[mark];
	//printf("valores = %d %f %d %d %s\n", mark, time_total[mark], start_line[mark], stop_line, filename);
	info.s_mark = mark;
	info.s_time = time_total[mark];
	info.s_start_line = start_line[mark];
	info.s_stop_line = stop_line;

	char only_filename[64];
	fname(only_filename, filename);
	strcpy(info.s_filename, only_filename);

	if ((int) write(fd_pipe, &info, sizeof(struct send_info)) == -1)
	{
		fprintf(stderr, RED "[Sprof]" RESET " Writing to the pipe has failed: %s\n", strerror(errno));
		exit(1);
	}
}

#ifndef _OPENMP
static pthread_t thr_start[MAX_ANNOTATIONS];

void _sprof_pthstart(pthread_t thr, int line, const char * filename)
{
	int l_mark = 0;
	static int max_mark = 0;

	if (count == 0)
		setconfig();

	while (l_mark < max_mark)
	{
		if (line != start_line[l_mark])
			l_mark++;
		else
		{
			GET_TIME(time_begin[l_mark]);
			break;
		}
	}

	if (l_mark == max_mark)
	{
		thr_start[l_mark] = thr;
		start_line[l_mark] = line;	
		max_mark++;
		GET_TIME(time_begin[l_mark]);
	}
}

void _sprof_pthstop(pthread_t thr_stop, int stop_line, const char * filename)
{
	struct send_info {
		int s_mark;
		float s_time;
		int s_start_line;
		int s_stop_line;
		char s_filename[64];
	} info;

	int l_mark = 0;

	while (true)
	{
		if (pthread_equal(thr_start[l_mark], thr_stop) != 0)
		{
			GET_TIME(time_final[l_mark]);
			break;
		}
		l_mark++;
	}

	time_total[l_mark] += time_final[l_mark] - time_begin[l_mark];
	info.s_mark = l_mark;
	info.s_time = time_total[l_mark];
	info.s_start_line = start_line[l_mark];
	info.s_stop_line = stop_line;

	char only_filename[64];
	fname(only_filename, filename);
	strcpy(info.s_filename, only_filename);

	write(fd_pipe, &info, sizeof(struct send_info));
}
#endif





