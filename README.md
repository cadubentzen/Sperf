# Sperf

The document is organized as shown below:

> SECTION 1 - What is sperf.
>
> SECTION 2 - How to use
>
> SECTION 3 - Instrumenting the code
>
> SECTION 4 - Configuring execution
>
> SECTION 5 - Options available

The entire suite is licensed under the GPL v3.0.

## SECTION 1 - What is sperf

**sperf** was developed by Márcio Jales (marcio.dojcosta@gmail.com) and continued by Vitor Ramos (ramos.vitor89@gmail.com), at UFRN (Universidade Federal do Rio Grande do Norte), as part of the undergraduate thesis in computer engineering.
Date: 04/05/2017. This document describes the essentials about the sperf toolsuite.

Sperf profiles an application by marking multiples regions on the source code and executes the application repeatedly, varying the input argument and number of threads according to the user configuration and from this is able to calculate the efficiency of the target.


## SECTION 2 - HOW TO USE

The user must instrument its target application before run with sperf, this is detailed in SECTION 3. Then configure the execution file informing how many tests will be made and the number of threads and the arguments to execute, more information on SECTION 4. Finally run `bin/./sperf app`.

## SECTION 3 - Instrumenting

To instrumenting manually including sperfops.h available on `include/sperfops.h` and use the function sperf_start(id) and 
sperf_stop(id) on the region of interest.

- `sperf_start`: receive 1 arguments, the identification of the region. It marks the beginning of the parallel area to be measured.
- `sperf_stop`: receive 1 arguments, the identification of the region. It marks the end of the parallel area to be measured.

Example:

```C++
#include "sperfops.h"
...
int main()
{
	sperf_start(1);
	#pragma omp parallel
	{
		...
	}
	sperf_start(1);
	...
}
```

Also this can be done automatically for openmp code using the command line : `./sperf -i -p teste/`, this will mark all `pragma omp parallel` regions on the cpp and c files on the folder teste. Also its possible to pass the extensions with the option `-e`. Example: `./sperf -i -p folder/ -e .hpp,.cpp,.c`

## SECTION 4 - Configuring

When the user call `./sperf app`, sperf will look for the file `etc/app.conf` to configure the execution, if this file doesn't exists it will be created with the same name as the application with the default configuration.

- `number_of_tests` : mandatory, it determines how many times the tests will run on the target application. Example:
  1. `number_of_tests=4`

- `list_threads_values`: optional, it consists of a list with the number of threads that should be used on each test, the values must be comma-separated, between keys and without spaces. Example:
  1. `list_threads_values={1,2,5,8,17}` will execute tests for 1, 2, 5, 8 and 17 threads, respectively. Note that the test with 1 thread will always be executed, so it does not have to be assigned in the list of values.

Also its possible to Uses a set of three variables to configure the list of threads, they are: `max_number_threads`, `type_of_step` and `value_of_step`. The first one dictates the upper limit of the number of threads that will be executed on the test. The second one may assume only two values: "power" and "constant". "power" means that the number of threads the target will run will be incremented in a geometric progression, while the "constant" option will increment the number in a arithmetic progression. The last variable gives the value to increment the number of thread on each step of the test. Consider, then, three examples below:

  1. `max_number_threads=16`, `type_of_step=power` and `value_of_step=2` will configure the target application to execute with 1, 2, 4, 8 and 16 threads.
  2. `max_number_threads=32`, `type_of_step=power` and `value_of_step=3` will configure the target application to execute with 1, 3, 9 and 27 threads.
  3. `max_number_threads=33`, `type_of_step=constant` and `value_of_step=4` will configure the target application to execute with 1, 5, 9, 13, 17, 21, 25, 29 and 33 threads.

- `list_of_args`: specify all the parameters to use. Example:
```
list_of_args={
arg1 arg2 ...,
arg1 arg2 ...,
arg1 arg2 ...,
arg1 arg2 ...
}
```

Will execute the program with each combination of threads and this arguments.

## SECTION 5 - Options available

The profiler is invoked by command line, as shown below:

`<path_to_profiler>/sperf <target> [OPTIONS]`
	
The options available to use with sperf is:

- `-t`: specify the output file type, there are three options available csv, xml, json.
  1. `sperf app -t csv`
  2. `sperf app -t xml`
  3. `sperf app -t json`

- `-o`: this option set the output filename.
  1. `sperf app -o outputfile`
  
- `-c`: this option is for choosing a different configuration file, by default if the option was not passed will load the file with the same name as the application.
  1. `sperf app -c my_config` using the configuration my_config.conf
  2. `sperf app` using the configuration app.conf
