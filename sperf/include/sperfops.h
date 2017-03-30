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

#ifndef _SPERFOPS_H_
#define _SPERFOPS_H_

#define GET_TIME(now) { \
   struct timeval t; \
   gettimeofday(&t, NULL); \
   now = t.tv_sec + t.tv_usec/1000000.0; \
}

#define	 MAX_ANNOTATIONS	64

#define sperf_start() _sperf_start(__LINE__, __FILE__);
#define sperf_stop() _sperf_stop(__LINE__, __FILE__);	
void _sperf_start(int line, const char * filename);
void _sperf_stop(int stop_line, const char * filename);

#ifndef _OPENMP

#define sperf_pthstart(thr_id) _sperf_pthstart(thr_id, __LINE__, __FILE__);
#define sperf_pthstop(thr_id) _sperf_pthstop(thr_id, __LINE__, __FILE__);
void _sperf_pthstart(pthread_t thr, int line, const char * filename);
void _sperf_pthstop(pthread_t thr, int stop_line, const char * filename);

#endif

void sperf_thrnum(int *valor);
#endif