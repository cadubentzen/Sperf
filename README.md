# Sperf

**sperf** was developed by Márcio Jales (marcio.dojcosta@gmail.com) and continued by Vitor Ramos (ramos.vitor89@gmail.com), at UFRN (Universidade Federal do Rio Grande do Norte), as part of the undegraduate thesis in computer engineering.
Date: 04/05/2017.

This document describes the essentials about the sperf toolsuite. It consists of:
- "sperf" binary executable (writter in C);
- "sperf_instr.sh" shell script;
- and the "libsperf.so" shared library.

The document is organized as shown below:

> SECTION 1 - What is sperf.
>
> SECTION 2 - How sperf works.
>
> SECTION 3 - INSTRUMENTING THE CODE
>
> SECTION 4 - AUTOMATIC INSTRUMENTATION
>
> SECTION 5 - CONFIGURING THE sperf EXECUTION
>
> SECTION 6 - EXECUTING sperf

The entire suite is licensed under the GPL v3.0.

## SECTION 1 - What is sperf

The main tool (the `sperf` binary executable, written in C) executes an user application (called "target") repeatedly, for the same size of problem, but different number of threads, so that it is able to calculate the speedup of the target. The user must, first of all, instrument its application using specific functions provided by the sperf library `libsperf.so`. If he/she wants it to be made automatically, there is an executable shell script called `sperf_instr.sh`  that may perform such task in certain conditions.  

That said, the tool suite aims two things, basically:

- First, a test plataform to create a profile about the speedup of multithread applications. This is acomplished by the binary executable called `sperf`.
- Second, to instrument the source-code of these applications automatically. This is acomplished by the shell script called `sperf_instr.sh`.

The tool was tested for OpenMP and Pthreads application.

## SECTION 2 - How sperf works

The user must instrument its target application that he/she wishes to be measured. In order to do so, he/she must use the library provided, named `libsperf.so`.

Next, the user executes sperf, passing, as arguments, the binary executable of the target application and all its necessary arguments. To configure how sperf will perform the tests, he/she must configure the file `<name_of_the_program>.conf`, where the file "name_of_the_program" is the name of the program that will run. There, it will be informed how many tests will be made and the number of threads that will be used and the arguments to execute.

## SECTION 3 - Instrumenting the code

In order to execute correctly, the functions provides by sperf library must be placed accordingly by the user (or using `sperf_instr.sh`. This will be explained on the next section). 

There are 5 functions available for this purpose:

1. sperf_start: does not return and receive 1 arguments, the identification of the region. It marks the beginning of the parallel area to be measured.
2. sperf_stop: does not return and receive 1 arguments, the identification of the region. It marks the end of the parallel area to be measured.

It is important to highlight that all the intrumentation works only OUTSIDE the parallel areas. sperf does not handle race conditions and other problems that arises inside parallel implementations.

------------------------------------------------------------------------------------

SECTION 4 - AUTOMATIC INSTRUMENTATION

Sometimes, looking into a code and placing instrumentation can be tricky. With that in mind, the shell script "sperf_instr.sh" was created as a first trial to automatically instrument code using the sperf library. Currently, it inserts only "sperf_start" and "sperf_stop" functions in OpenMP directives that start with "pragma omp parallel".

To use this functionality, the command line consists of:
	./sperf -i -p folder/
There is still one more option that can configure what extentions sperf will look for -e.
	./sperf -i -p folder/ -e .cpp,.h,.c

------------------------------------------------------------------------------------

SECTION 5 - CONFIGURING THE sperf EXECUTION

The configuration of the profiler's execution is controlled by the "sperf_exec.conf" configuration file. There are six variables that may be used in two distinct ways to configure sperf. For both, the use of "number_of_tests" is mandatory. It determines how many times the tests will run on the target application.	

Thus, the first possibility of configuration uses the "list_threads_values" variable. It consists of a list with the number of threads that should be uses on each test. The values must be comma-separated, between keys and without spaces. For example, the declaration "list_threads_values={2,5,8,17}" will execute tests for 2, 5, 8 and 17 threads, respectively. Note that the test with 1 thread will always be executed, so it does not have to be assigned in the list os values.

The second possibility uses a set of three variables: "max_number_threads", "type_of_step" and "value_of_step". The first one dictates the upper limit of the number of threads that will be executed on the test. The second one may assume only two values: "power" and "constant". "power" means that the number of threads the target will run will be incremented in a geometric progression fashion, while the "constant" option will increment the number in a arithmetic progression fashion. The last variable gives the value to increment the number of thread on each step of the test. Consider, then, three examples below:

	1 - The set "max_number_threads=16", "type_of_step=power" and "value_of_step=2" will configure the target application to execute with 1, 2, 4, 8 and 16 threads;
	
	2 - The set "max_number_threads=32", "type_of_step=power" and "value_of_step=3" will configure the target application to execute with 1, 3, 9 and 27 threads;
	
	3 - The set "max_number_threads=33", "type_of_step=constant" and "value_of_step=4" will configure the target application to execute with 1, 5, 9, 13, 17, 21, 25, 29 and 33 threads.

The list_of_arguments parameter is usend to run with multiple arguments. For example, the declaration
"list_of_args={
arg1 arg2 ...,
arg1 arg2 ...,
arg1 arg2 ...,
arg1 arg2 ...
}"
Will execute the program with wich argument 

------------------------------------------------------------------------------------

SECTION 6 - EXECUTING sperf

The profiler is invoked by command line, as shown below:

	<path_to_profiler>/sperf [OPTIONS] <target> <target's arguments>
	
The options available to use with sperf is:

	-t this option means that the target application has one of its arguments consisting of the number of threds that it will run. Thus, consider the next examples:
	1 - An application named "app" has three arguments, where the second represents the number of threads that "app" will fork to execute. The option "-t" may be used then: "sperf -t 2 app arg1 arg2 arg3";
	
	2 - An application named "foo" has four arguments, where the fourth represents the number of threads that "foo" will fork to execute. The option "-t" may be used then: "sperf -t 4 foo arg1 arg2 arg3 arg4";
	
	3 - An application named "bar" has six arguments, where none of them represents the number of threads. The option "-t" may NOT be used then: "sperf bar arg1 arg2 arg3 arg4 arg5 arg6".
	
	-o this option set the output to csv file the argument passed is the name of the file, example:
	1 - sperf -o speedup app arg1 arg2
	
	-c this option is for choosing a different configuration file, by default if the option was not passed the configuration file is sperf_exec, example:
	1 - sperf -c my_config app arg1
	2 - sperf app arg1
	

	Notice that if the user inserts the function "sperf_thrnum" (refer to section 3) in his/her application, "-t" isn't necessary even if the target passes the number of threads in its arguments.
