#ifndef _SPROFOPS_H_
#define _SPROFOPS_H_

#define GET_TIME(now) { \
   struct timeval t; \
   gettimeofday(&t, NULL); \
   now = t.tv_sec + t.tv_usec/1000000.0; \
}

#define	 MAX_ANNOTATIONS	64

#define sprof_start() _sprof_start(__LINE__, __FILE__);
#define sprof_stop() _sprof_stop(__LINE__, __FILE__);	
void _sprof_start(int line, const char * filename);
void _sprof_stop(int stop_line, const char * filename);

#ifndef _OPENMP

#define sprof_pthstart(thr_id) _sprof_pthstart(thr_id, __LINE__, __FILE__);
#define sprof_pthstop(thr_id) _sprof_pthstop(thr_id, __LINE__, __FILE__);
void _sprof_pthstart(pthread_t thr, int line, const char * filename);
void _sprof_pthstop(pthread_t thr, int stop_line, const char * filename);

#endif

void sprof_thrnum(int *valor);
#endif
