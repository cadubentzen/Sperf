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
#include "../include/sprofops.h"

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

int * list_of_threads_value;
int optset = 0;
int num_marks;
float prl_times[MAX_ANNOTATIONS];
int start_line[MAX_ANNOTATIONS], stop_line[MAX_ANNOTATIONS];
char **fname = (char **) malloc(MAX_ANNOTATIONS);


void set_profcfg(int val, int op)
{
	char *str = (char *) malloc(7*sizeof(char));

	if (str == NULL)
   	{
      		fputs(RED "[Sprof]" RESET " Alocation error", stderr);
      		exit(1);
   	}
  	snprintf(str, 7, "%d", val);
   	if (op == SET_THREADS)
   	{
     		setenv("NUM_THRS_ATUAL", str, 1);
      		setenv("OMP_NUM_THREADS", str, 1);
   	}
   	else if (op == SET_PIPE)
      		setenv("FD_PIPE", str, 1);
   	else
   	{
     		fputs(RED "[Sprof]" RESET " Invalid option in set_profcfg", stderr);
      		exit(1);
   	}
   	free(str);
}


char * get_path(char * argmnt, int location)
{
	const char * delim = "/";
	char * token, * argv0;
	char * path = (char *) malloc(200);
	int i = 0,  j;
	
	argv0 = strdup(argmnt);
	token = strtok(argv0, delim);
	while( token != NULL ) 
   	{
		i++;  
		token = strtok(NULL, delim);
  	 }

	j = i;
	argv0 = strdup(argmnt);
	token = strtok(argv0, delim);
	strcpy(path, token);
	while( token != NULL ) 
   	{
		j--;
		if ( j == 1)
			break;
		strcat(path, "/");    
		token = strtok(NULL, delim);
		strcat(path, token);
  	 }
	
	if (location == ETC_PATH)	
		strcat(path, "/../etc/sprof_exec.conf");
	if (location == RESULT_PATH)	
		strcat(path, "/../results/");

	return path;
}


void exec_conf(int * l_num_exec, int * l_num_max_threads, char * exec_path)
{
	char * step_type = (char *) malloc(10);
	char * str = (char *) malloc(50);
	char * str_ant = (char *) malloc(50);
	char * config_path = (char *) malloc(200);
	int step, max_threads;
	int flag_list, flag_max_threads = 0, flag_step_type = 0, flag_step_value = 0, flag_num_tests = 0;
	const char * delim = ",";
	FILE * conf_file;

	config_path = get_path(exec_path, ETC_PATH);

	if ((conf_file = fopen(config_path, "r")) == NULL)
	{
		fprintf(stderr, RED "[Sprof]" RESET " Failed to open sprof_exec.conf: %s\n", strerror(errno));
		exit(1);
	}

	strcpy(str, "str_ant");
	strcpy(str_ant, str);
	fscanf(conf_file, "%s", str);

	while (strcmp(str, str_ant) != 0 || (strcmp(str, "#") == 0 && strcmp(str_ant, "#") == 0))
	{
		if (str[0] == '#')
		{
			while (fgetc(conf_file) == 32)
			{
				strcpy(str_ant, str);
				fscanf(conf_file, "%s", str); 
			}
		}
		else
		{
			if (strncmp(str, "number_of_tests=", 16) == 0)
			{
				printf(BLUE "[Sprof]" RESET " Retrieving number of testes...\n");
				if (strlen(str) == 16)
				{
					fputs(RED "[Sprof]" RESET " You must specify a number of tests\n", stderr);
      					exit(1);
				}
				else
				{
					fseek(conf_file, -2, SEEK_CUR);
					while(fgetc(conf_file) != 61)
						fseek(conf_file, -2, SEEK_CUR);
					fscanf(conf_file, "%d", l_num_exec);
					flag_num_tests = 1;
				}
			}
			else if (strncmp(str, "list_threads_values=", 20) == 0)
			{	
				if (strlen(str) == 20 || strcmp(str, "list_threads_values={}") == 0)
					flag_list = 0;
				else
				{
					printf(BLUE "[Sprof]" RESET " Retrieving the list of threads values...\n");
					if (str[strlen(str)-1] != '}')
					{
						fputs(RED "[Sprof]" RESET " Format error on list_threads_values\n", stderr);
	      					exit(1);
					}
					fseek(conf_file, -2, SEEK_CUR);
					while(fgetc(conf_file) != 61)
						fseek(conf_file, -2, SEEK_CUR);
					if (fgetc(conf_file) != 123)
					{
						fputs(RED "[Sprof]" RESET " Format error on list_threads_values\n", stderr);
	      				exit(1);
					}
					else
					{
						char * token;
						int i = 1;
						fscanf(conf_file, "%s", str);
						token = strtok(str, delim);
						while( token != NULL ) 
					   	{
							if (i == 1)
								list_of_threads_value = (int *) malloc(sizeof(int));
							else
								list_of_threads_value = (int *) realloc(list_of_threads_value, i * sizeof(int));
							sscanf(token, "%d", &list_of_threads_value[i-1]);
							token = strtok(NULL, delim);
							i++;
					   	}
						*l_num_max_threads = list_of_threads_value[i-2];
					}
					flag_list = 1;		
				}
			}
			else if (strncmp(str, "max_number_threads=", 19) == 0)
			{
				if (flag_list != 1)
				{
					if (strlen(str) == 19)
					{
						fputs(RED "[Sprof]" RESET " You must specify a maximum number of threads\n", stderr);
      						exit(1);
					}
					else
					{
						fseek(conf_file, -2, SEEK_CUR);
						while(fgetc(conf_file) != 61)
							fseek(conf_file, -2, SEEK_CUR);
						fscanf(conf_file, "%d", &max_threads);
						flag_max_threads = 1;
					}
				}
			}
			else if (strncmp(str, "type_of_step=", 13) == 0)
			{
				if (flag_list != 1)
				{
					if (strlen(str) == 13)
					{
						fputs(RED "[Sprof]" RESET " You must specify a step method to increment the number of threads\n", stderr);
      					exit(1);
					}
					else
					{
						fseek(conf_file, -2, SEEK_CUR);
						while(fgetc(conf_file) != 61)
							fseek(conf_file, -2, SEEK_CUR);
						fscanf(conf_file, "%s", step_type);
						if ((strncmp(step_type, "constant", 8) != 0  && strncmp(step_type, "power", 5) != 0 ) || (strlen(step_type) != 8 && strlen(step_type) != 5))
						{
							fputs(RED "[Sprof]" RESET " Invalid step method\n", stderr);
      							exit(1);
						}
						flag_step_type = 1;
					}
				}
			}
			else if (strncmp(str, "value_of_step=", 14) == 0)
			{
				if (flag_list != 1)
				{
					if (strlen(str) == 14)
					{
						fputs(RED "[Sprof]" RESET " You must specify a step value to increment the number of threads\n", stderr);
      						exit(1);
					}
					else
					{
						fseek(conf_file, -2, SEEK_CUR);
						while(fgetc(conf_file) != 61)
							fseek(conf_file, -2, SEEK_CUR);
						fscanf(conf_file, "%d", &step);
						flag_step_value = 1;
					}
				}
			}
			else
			{
				fputs(RED "[Sprof]" RESET " Invalid configuration variable\n", stderr);
      				exit(1);	
			}
		}
		strcpy(str_ant, str);
		fscanf(conf_file, "%s", str);
	}
	if (flag_num_tests == 0)
	{
		fputs(RED "[Sprof]" RESET " 'number_of_tests' variable missing\n", stderr);
      	exit(1);	
	} 
	if (flag_list == 0 && (flag_max_threads == 0 || flag_step_value == 0 || flag_step_type== 0))
	{
		fputs(RED "[Sprof]" RESET " The number of threads to be executed must be set properly.\n", stderr);
		fputs(RED "[Sprof]" RESET " Define 'list_values_threads' variable or the set of three variables 'max_number_threads', 'type_of_step' and 'value_of_step'\n", stderr);
      		exit(1);	
	}
	if (flag_list == 0)
	{
		printf(BLUE "[Sprof]" RESET " Retrieving the list of threads values...\n");
		
		int k = 0;
		list_of_threads_value = (int *) malloc(sizeof(int));	

		if (strncmp(step_type, "constant", 8) == 0)
		{
			if (k == 0)
			{
				list_of_threads_value[k] = 1 + step;
				k++;
			}
			while (list_of_threads_value[k-1] < max_threads)
			{
				list_of_threads_value = (int *) realloc(list_of_threads_value, (k+1) * sizeof(int));
				list_of_threads_value[k] = list_of_threads_value[k-1] + step;
				k++;
			}
		}
		if (strncmp(step_type, "power", 5) == 0)
		{
			if (k == 0)
			{
				list_of_threads_value[k] = 1 * step;
				k++;
			}
			while (list_of_threads_value[k-1] < max_threads)
			{
				list_of_threads_value = (int *) realloc(list_of_threads_value, (k+1) * sizeof(int));
				list_of_threads_value[k] = list_of_threads_value[k-1] * step;
				k++;
			}
		}
		*l_num_max_threads = max_threads;
	}
	if (fclose(conf_file) != 0)
	{
		fprintf(stderr, RED "[Sprof]" RESET " Failed to close sprof_exec.conf\n");
		exit(1);
	}	

	free(step_type);
	free(str);
	free(str_ant);
	free(config_path);
}


void time_information(float *l_time_singleThrPrl, int cur_thrs, double l_end, double l_start, FILE * out, int cur_exec, char *l_args[], int l_argc)
{
   	static float time_singleThrTotal;
   	int count, i;

   	if (cur_thrs == 1)
   	{
	      for (count = 0; count < num_marks; count++)
		 	l_time_singleThrPrl[count] = prl_times[count];

	      time_singleThrTotal = (float) (l_end - l_start);
		fprintf(out, "\n-----> Execution number %d for %s:\n", cur_exec + 1, l_args[1]);
   	}
	
   	fprintf(out, "\n\t--> Result for %d threads, application %s, arguments: ", cur_thrs, l_args[1]);
	if (optset != 0)
		l_argc -= 2;
	for (i = 2; i < l_argc; i++)
	{
		if (i != optset)
			fprintf(out, "%s, ", l_args[i]);
		if (i == l_argc - 1)
			fprintf(out, "\n");
	}
	
   	for (count = 0; count < num_marks; count++)
   	{
     		fprintf(out, "\n\t\t Parallel execution time of the region %d, lines %d to %d on file %s: %f seconds\n", count+1, start_line[count], stop_line[count], fname[count], prl_times[count]);
     		fprintf(out, "\t\t Speedup for the parallel region %d: %f\n", count+1, l_time_singleThrPrl[count]/prl_times[count]);
   	}

   	fprintf(out, "\n\t\t Total time of execution: %f seconds\n", l_end - l_start);
   	fprintf(out, "\t\t Speedup for the entire application: %f\n", time_singleThrTotal/(float)(l_end - l_start));
}


int main(int argc, char *argv[])
{
   	double start, end;
   	float time_singleThrPrl[MAX_ANNOTATIONS];
   	int i = 0, j, num_exec, current_exec, num_current_threads, num_max_threads;
   	/* pipes[0] = leitura; pipes[1] = escrita */
   	int pipes[2];
	char * filename = (char *) malloc(30);
	char * result_file = (char *) malloc(200);
	char **args;
	int mark, mark_ant = 0;
	time_t rawtime = time(NULL);
	struct tm *local = localtime(&rawtime);
   	FILE *out;

	struct recv_info {
		int s_mark;
		float s_time;
		int s_start_line;
		int s_stop_line;
		char s_filename[64];
	} info;

	if (argc == 1)
	{
		printf(RED "[Sprof]" RESET " Target application missing\n");
		exit(1);
	}	
	printf(BLUE "[Sprof]" RESET " Reading sprof_exec.conf\n");

	exec_conf(&num_exec, &num_max_threads, argv[0]);

	sprintf(filename, "%d-%d-%d-%dh%dm%ds.txt", local->tm_mday, local->tm_mon + 1, local->tm_year + 1900, local->tm_hour, local->tm_min, local->tm_sec);
	result_file = get_path(argv[0], RESULT_PATH);
	strcat(result_file, filename);

	for (j = 0; j < MAX_ANNOTATIONS; j++)
		fname[j] = (char *) malloc(64);
   	
	/* Passando os argumentos da linha de comando para a variável args */
	if (strcmp(argv[1], "-t") != 0)
	{ 
		args = (char **) malloc((argc + 1)*sizeof(char*));
		printf(BLUE "[Sprof]" RESET " sprof_thrnum function required\n");
	  	while (i <= argc)
	   	{
		      args[i] = (char *)malloc(100*sizeof(char));
		      if (i == argc)
		  		args[i] = (char *) NULL;
	      	else
		  		strcpy(args[i], argv[i]);
	      	i++;
	   	}
	}
	else
	{
		args = (char **) malloc((argc - 1)*sizeof(char*));
		printf(BLUE "[Sprof]" RESET " Thread value passed by command line argument to the target application. sprof_thrnum function not required\n");
	  	while (i <= argc - 2)
	   	{
		      args[i] = (char *)malloc(100*sizeof(char));
		      if (i == argc - 2)
			  	args[i] = (char *) NULL;
		      else if (i == atoi(argv[2]) + 1)
		  		optset = i;
			else
				strcpy(args[i], argv[i+2]);
	      	i++;
	   	}
	}

	for (current_exec = 0; current_exec < num_exec; current_exec++)
	{  
   		num_current_threads = 1;
		int j = 0;
		printf(BLUE "[Sprof]" RESET " Current execution %d of %d\n", current_exec + 1, num_exec);

   		while (num_current_threads <= num_max_threads)
   		{
      		pid_t pid_child;

			printf(BLUE "[Sprof]" RESET " Executing for %d threads\n", num_current_threads);
      		set_profcfg(num_current_threads, SET_THREADS);
		  	if (pipe(pipes) == -1)
		  	{
		     		fprintf(stderr, RED "[Sprof]" RESET " IPC initialization error: %s\n", strerror(errno));
		     		exit(1);
		  	}
		  	set_profcfg(pipes[1], SET_PIPE);
		  	if ((pid_child = fork()) < 0)
		  	{
		     		fprintf(stderr, RED "[Sprof]" RESET " Failed to fork(): %s\n", strerror(errno));
		     		exit(1);
		  	}
		 	GET_TIME(start);

		  	/* Se for o processo filho, argv[1] (o nome da aplicação passada por linha de comando) é executado, com os argumentos
		     	especificados pelo vetor de strings args + 1 (o primeiro elemento de args é ./Sprof. Portanto, é descartado) */
		  	if (pid_child == 0)
		  	{
		     		if (close(pipes[0]) == -1)
		     		{
		        		fprintf(stderr, RED "[Sprof]" RESET " Failed to close() IPC: %s\n", strerror(errno));
		        		exit(1);
		     		}
				if (optset != 0)
					sprintf(args[optset], "%d", num_current_threads);
			     	if (execv(args[1], (args + 1)) == -1)
			   	{
				  	fprintf(stderr, RED "[Sprof]" RESET " Failed to start the target application: %s\n", strerror(errno));
				  	exit(1);
			     	}
		  	}
		  	else
		  	{
				if (close(pipes[1]) == -1)
			    {
					 fprintf(stderr, RED "[Sprof]" RESET " Failed to close IPC: %s\n", strerror(errno));
					 exit(1);
			    }
				while (!waitpid(pid_child, 0, WNOHANG)) 
				{
					if ((int) read(pipes[0], &info, sizeof(struct recv_info)) == -1)
					{
						fprintf(stderr, RED "[Sprof]" RESET " Reading from the pipe has failed: %s\n", strerror(errno));
						exit(1);
					}	

					mark = info.s_mark;
					prl_times[mark] = info.s_time;
					start_line[mark] = info.s_start_line;
					stop_line[mark] = info.s_stop_line;
					strcpy(fname[mark], info.s_filename);

					if (num_current_threads == 1)
					{
						if (mark > mark_ant)
						{
							mark_ant = mark;
							num_marks = mark + 1;
						}
					}
				}		
			    GET_TIME(end);			

		 		if ((out = fopen(result_file, "a")) == NULL)
		     	{
					fprintf(stderr, RED "[Sprof]" RESET " Failed to open the result file: %s\n", strerror(errno));
		        	exit(1);
		     	}

				time_information(time_singleThrPrl, num_current_threads, end, start, out, current_exec, args, argc);

				if (fclose(out) != 0)
				{
			      	fprintf(stderr, RED "[Sprof]" RESET " Failed to close the result file\n");
		        	exit(1);
				}
			     if (close(pipes[0]) == -1)
			     {
				  	fprintf(stderr, RED "[Sprof]" RESET " Failed to close IPC: %s\n", strerror(errno));
				  	exit(1);
			     }
		  	}
			if (num_current_threads == num_max_threads)
				break;
			num_current_threads = list_of_threads_value[j];
			j++;
	   	}
	}
	
	i = 0;
	if (strcmp(argv[1], "-t") != 0)
	{
		while (i < argc)
		{
			free(args[i]);
			i++;
		}
	}
	else
	{
		while (i < argc - 2)
		{
			free(args[i]);
			i++;
		}
	}
	for (j = 0; j < MAX_ANNOTATIONS; j++)
		free(fname[j]);

	free(fname);
	free(args);
	free(fname);
	free(list_of_threads_value);
	free(filename); 
	free(result_file);
	
	return 0;
}
