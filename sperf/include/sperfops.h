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
#define _SPERFOPS_H_

#pragma once
#include <pthread.h>
#include <sys/time.h>

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

#define sperf_start(id) _sperf_start(id, __LINE__, __FILE__);
#define sperf_stop(id) _sperf_stop(id, __LINE__, __FILE__);
void _sperf_start(int id, int start_line, const char * filename);
void _sperf_stop(int id, int stop_line, const char * filename);

void setconfig();

#ifndef _OPENMP

#define sperf_pthstart(thr_id) _sperf_pthstart(thr_id, __LINE__, __FILE__);
#define sperf_pthstop(thr_id) _sperf_pthstop(thr_id, __LINE__, __FILE__);
void _sperf_pthstart(pthread_t thr, int start_line, const char * filename);
void _sperf_pthstop(pthread_t thr, int stop_line, const char * filename);

#endif

void sperf_thrnum(int *valor);

#endif
