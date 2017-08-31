#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>

int sum(int c)
{
	int count = 0, s = 0;
	while(count < 10) {
		s = s + c;
		count++;
	}
	return s;
};

void prod(int s, int c)
{
	int count = 0;
	while(count < 12){
		s = s*c;
		count++;
	}
};

int main(char** argv, int argc)
{
	int res, i;
	int *t = malloc(16);
	sleep(3);
	res = sum(argc);
	//printf("Cheio\n");
	/*#pragma omp parallel num_threads(3)
	{
		int tid = omp_get_thread_num();
    #pragma omp single nowait
		{
			printf("Thread %d in #1 single construct.\n", tid);
		}
    #pragma omp single nowait
		{
			printf("Thread %d in #2 single construct.\n", tid);
		}
    #pragma omp single nowait
		{
			printf("Thread %d in #3 single construct.\n", tid);
		}
	}*/
	# pragma omp parallel for num_threads(2)
	for(i = 0; i < 400000000; i++) {
		prod(res, argc);
		//printf("sleep %ld\n", syscall(SYS_gettid));
		if(i % 10 == 0) {
			res = 0;
			//printf("i mod 10\n");
		}
	}

	return 0;
}
