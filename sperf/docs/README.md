# SPerf

**SPerf** was developed by Márcio Jales (marcio.dojcosta@gmail.com) and continued by Vitor Ramos (ramos.vitor89@gmail.com), at UFRN (Universidade Federal do Rio Grande do Norte), as part of the undegraduate thesis in computer engineering.
Date: 08/10/2017.

This document describes the essentials about the sperf toolsuite. It is organized as shown below:

> SECTION 1 - What is sperf.
>
> SECTION 2 - How sperf works.
>
> SECTION 3 - Instrumenting the code
>
> SECTION 4 - Automatic instrumentation
>
> SECTION 5 - Configuring sperf execution
>
> SECTION 6 - Executing sperf

The entire suite is licensed under the GPL v3.0.

## SECTION 1 - What is sperf

The main tool (the `sperf` binary executable, written in C) executes an user application (called "target") repeatedly, for the same size of problem, but different number of threads, so that it is able to calculate the speedup of the target. The user must, first of all, instrument its application using specific functions provided by the sperf header `sperfops.h`.

That said, the tool suite aims two things, basically:

1. Being a test plataform to create a profile about the speedup of multithread applications. This is acomplished by the binary executable called `sperf`.
2. Instrument the source-code of these applications automatically. This is acomplished by running sperf and specifying the folder where the source code is present.

The tool was tested for OpenMP and Pthreads application.

## SECTION 2 - How sperf works

The user must instrument its target application that he/she wishes to be measured. In order to do so, he/she must include `sprefops.h` and use the functions it provides.

Next, the user executes sperf, passing, as arguments, the binary executable of the target application and all its necessary arguments. To configure how sperf will perform the tests, he/she must configure the file `<name_of_the_program>.conf`, where the file "name_of_the_program" is the name of the program that will run. There, it will be informed how many tests will be made and the number of threads that will be used and the arguments to execute.

## SECTION 3 - Instrumenting the code

In order to execute correctly, the functions provided by sperf must be placed accordingly by the user (or leave this task to the automatic instrumentation. This will be explained on the next section). 

There are 2 functions available for this purpose:

- `sperf_start`: does not return and receive 1 argument, the identification of the region. It marks the beginning of the parallel area to be measured.
- `sperf_stop`: does not return and receive 1 argument, the identification of the region. It marks the end of the parallel area to be measured.

It is important to highlight that all the intrumentation works only OUTSIDE the parallel areas. sperf does not handle race conditions and other problems that arises inside parallel implementations.

## SECTION 4 - Automatic instrumentation

Sometimes, looking into a code and placing instrumentation can be tricky. With that in mind, `sperf` provides two option flags:

- `-i`: it defines that `sperf` will instrument the code inside a given folder.
- `-p`: mandatory flag. The user defines in which folder resides the code to be instrumented. Relative and absolute path may be given.
- `-e`: if the user wishes to define only certain file extensions to be instrumented.

To use this functionality, then, there are two command line possibilities: 

1. `./sperf -i -p folder/`
2. `./sperf -i -p folder/ -e .cpp,.h,.c`

Currently, it inserts only `sperf_start` and `sperf_stop` functions in OpenMP directives that start with `pragma omp parallel`.

## SECTION 5 - Configuring sperf execution

The configuration of the profiler's execution is controlled by the `sperf_default_exec.conf` configuration file. There are six variables that may be used in two distinct ways to configure sperf. For both, the use of `number_of_tests` is mandatory. It determines how many times the tests will run on the target application.	

Thus, the first possibility of configuration uses the `list_threads_values` variable. It consists of a list with the number of threads that should be uses on each test. The values must be comma-separated, between keys and without spaces. For example, the declaration `list_threads_values={2,5,8,17}` will execute tests for 2, 5, 8 and 17 threads, respectively. Note that the test with 1 thread will always be executed, so it does not have to be assigned in the list os values.

The second possibility uses a set of three variables: `max_number_threads`, `type_of_step` and `value_of_step`. The first one dictates the upper limit of the number of threads that will be executed on the test. The second one may assume only two values: "power" and "constant". "power" means that the number of threads the target will run will be incremented in a geometric progression fashion, while the "constant" option will increment the number in a arithmetic progression fashion. The last variable gives the value to increment the number of thread on each step of the test. Consider, then, three examples below:

- The set `max_number_threads=16`, `type_of_step=power` and `value_of_step=2` will configure the target application to execute with 1, 2, 4, 8 and 16 threads;
- The set `max_number_threads=32`, `type_of_step=power` and `value_of_step=3` will configure the target application to execute with 1, 3, 9 and 27 threads;
- The set `max_number_threads=33`, `type_of_step=constant` and `value_of_step=4` will configure the target application to execute with 1, 5, 9, 13, 17, 21, 25, 29 and 33 threads.

The list_of_arguments parameter is usend to run with multiple arguments. For example, the declaration
```
list_of_args={
arg1 arg2 ...,
arg1 arg2 ...,
arg1 arg2 ...,
arg1 arg2 ...
}
```
Will execute the program with each argument.

It is possible to pass another configuration file, instead of the default one. This functionality is explained in the next section.

## SECTION 6 - Executing sperf

The profiler is invoked by command line, as shown below:

`<path_to_profiler>/sperf [OPTIONS] <target> <target's arguments>`
	
The options available to use with sperf are:

- `-t`: this option means that the target application has one of its arguments consisting of the number of threds that it will run. Thus, consider the next examples:
   1. An application named "app" has three arguments, where the second represents the number of threads that "app" will fork to execute. The option `-t` may be used then: `sperf -t 2 app arg1 arg2 arg3`;
   2. An application named "foo" has four arguments, where the fourth represents the number of threads that "foo" will fork to execute. The option `-t` may be used then: `sperf -t 4 foo arg1 arg2 arg3 arg4`;
   3. An application named "bar" has six arguments, where none of them represents the number of threads. The option `-t` may NOT be used then: `sperf bar arg1 arg2 arg3 arg4 arg5 arg6`.
   
- `-o`: this option set the output to csv file the argument passed is the name of the file, example: `sperf -o speedup app arg1 arg2`
	
- `-c`: this option is for choosing a different configuration file, by default if the option was not passed the configuration file is `sperf_default_exec.conf`. Example:
  1. `sperf -c my_config app arg1`
  2. `sperf app arg1` (`sperf_default_exec.conf` is read.)
	
~~Notice that if the user inserts the function `sperf_thrnum` (refer to section 3) in his/her application, `-t` isn't necessary even if the target passes the number of threads in its arguments.~~
