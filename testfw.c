#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <unistd.h>
#include <signal.h>
#include "testfw.h"

/* ********** STRUCTURES ********** */

/**
 * @brief test structure
 */
struct testfw_t	{
	struct test_t ** tests;	//Array that will contains every tests of the framework
	int nb_tests;	//Integer value with the numbers of tests that has been registered

	char * program;
	char * logfile;	//The logfile where we want to save the output
	char * cmd;	//The cmd where we want to redirect the output

	int timeout;	//The program run timeout value

	bool silent;	//Silent mode (no text displayed)
	bool verbose;	//Verbose mode (everything displayed)
};

/* ********** FRAMEWORK ********** */

/**
 * @brief initialize test framework
 *
 * @param program the filename of this executable
 * @param timeout the time limits (in sec.) for each test, else 0.
 * @param logfile the file in which to redirect all test outputs (standard & error), else NULL
 * @param cmd a shell command in which to redirect all test outputs (standard & erro),else NULL
 * @param silent if true, the test framework runs in silent mode
 * @param verbose if true, the test framework runs in verbose mode
 * @return a pointer on a new test framework structure
 */
struct testfw_t *testfw_init(char *program, int timeout, char *logfile, char *cmd, bool silent, bool verbose) {
	//printf("Program: %s\n timeout=%d\n logfile=%s\n cmd=%s\n", program, timeout, logfile, cmd);
	struct testfw_t* res = (struct testfw_t*) malloc(sizeof(struct testfw_t));
	if(!res){
		fprintf(stderr, "Invalid memory allocation\n");
		free(res);
		return NULL;
	}

	res->tests = (struct test_t **) malloc(sizeof(struct test_t *));
	if(!res->tests){
		fprintf(stderr, "Invalid memory allocation to save the tests array\n");
		testfw_free(res);
		return NULL;
	}

	res->nb_tests = 0;

	res->program = NULL;
	res->logfile = NULL;
	res->cmd = NULL;

	if(cmd != NULL){
		asprintf(&res->cmd, "%s", cmd);
	}
	if(program != NULL){
		asprintf(&res->program, "%s", program);
	}
	if(logfile != NULL){
		asprintf(&res->logfile, "%s", logfile);
	}

	if(timeout >= 0)
		res->timeout = timeout;
	else
		res->timeout = 2;	//If no good timeout value passed as parameters, set to default (ie. 2 seconds)


	if (verbose == true)
		res->verbose = true;
	else
		res->verbose = false;


	if (silent == true){
		res->silent = true;
		res->verbose = false;
	} else {
		res->silent = false;
	}


	return res;
}

/**
 * @brief finalize the test framework and free all memory
 *
 * @param fw the test framework to be freed
 */
void testfw_free(struct testfw_t *fw) {
	for(unsigned int i = 0; i < fw->nb_tests; i++){	//Free each tests one by one
		if(fw->tests[i]){
			free(fw->tests[i]->suite);
			free(fw->tests[i]->name);
			free(fw->tests[i]);
		}
	}

	if(fw->tests)
		free(fw->tests);

	if(fw->cmd)
		free(fw->cmd);
	if(fw->logfile)
		free(fw->logfile);

	free(fw->program);

	free(fw);
}

/**
 * @brief get number of registered tests
 *
 * @param fw the test framework
 * @return the number of registered tests
 */
int testfw_length(struct testfw_t *fw)	{
	return fw->nb_tests	;
}

/**
 * @brief get a registered test
 *
 * @param fw the test framework
 * @param k index of the test to get (k >=0)
 * @return a pointer on the k-th registered test
 */
struct test_t *testfw_get(struct testfw_t *fw, int k) {
	if(k < fw->nb_tests && k >= 0)	//Check if the nb of test is positive or 0 and is inferior to the number of tests registered
		return fw->tests[k];
	else
		return NULL;
}

/* ********** REGISTER TEST ********** */

/**
 * @brief register a single test function named "<suite>_<name>""
 *
 * @param fw the test framework
 * @param suite a suite name in which to register this test
 * @param name a test name
 * @return a pointer to the structure, that registers this test
 */
struct test_t *testfw_register_func(struct testfw_t *fw, char *suite, char *name, testfw_func_t func)	{
	struct test_t *res = (struct test_t *) malloc(sizeof(struct test_t));

	char * suite1 = NULL;
	char * name1 = NULL;

	int res_suite = asprintf(&suite1, "%s",suite);
	int res_name = asprintf(&name1, "%s",name);

	if(res_suite < 0 || res_name < 0){
		free(res);
		return NULL;
	}
	testfw_func_t * fun = malloc(sizeof(func));

	*fun = func;

	res -> suite = suite1;
	res -> name = name1;
	res -> func = *fun;

	struct test_t **tmp = (struct test_t**)realloc(fw->tests, (fw->nb_tests+1)*sizeof(struct test_t*));
	if (tmp != NULL) {
			fw->tests = tmp;
	} else {
		free(res);
		free(tmp);
		free(fun);
		free(suite1);
		free(name1);
		return NULL;
	}

	fw->tests[fw->nb_tests] = res;
	fw->nb_tests += 1;

	return res;
}

/**
 * @brief register a single test function named "<suite>_<name>""
 *
 * @param fw the test framework
 * @param suite a suite name in which to register this test
 * @param name a test name
 * @return a pointer to the structure, that registers this test
 */
struct test_t *testfw_register_symb(struct testfw_t *fw, char *suite, char *name) {

	if(!suite || !name || !fw)
		return NULL;

	char *test_name = NULL;
	int testName_res = asprintf(&test_name, "%s_%s",suite,name);

	if(testName_res <0 || !test_name)
		return NULL;

	void * handle_sym = dlopen(fw->program, RTLD_NOW);
	void * (*func) (int argc, char*argv);

	dlerror();	//clear error code
	func =	dlsym(handle_sym, test_name);	//Return null if no matches

	return testfw_register_func(fw, suite, name, (testfw_func_t) func);
}


 /**
  * @brief register all test functions named "<suite>_*"
  *
  * @param fw the test framework
  * @param suite a suite name in which to register these tests
  * @return the number of new registered tests
  */
int testfw_register_suite(struct testfw_t *fw, char *suite) {
	if(!suite || !fw || !fw->program)
		return 0;

	char * cmd = NULL;
	int cmd_res = asprintf(&cmd, "nm --defined-only %s | cut -d ' ' -f 3 | grep \"^%s_\"",fw->program, suite);
	if(cmd_res < 0 || !cmd)
		return 0;

	FILE * f = popen(cmd, "r");	//Exec the cmd and store the result in FILE * f
	assert(f);
	char path[1024];

	unsigned int sum					= 0;
	unsigned int suite_length = 0;
	if(suite)
		suite_length = strlen(suite);
	unsigned int path_length	= 0;

	while(fgets(path, sizeof(path) -1, f) != NULL){		//get the output line per line in path variable
		unsigned int name_length	= 0;
		if(path != NULL)
			path_length = strlen(path);

		name_length = (path_length - suite_length);

		char * name = (char*) malloc(sizeof(char) * (name_length));
		assert(name);

		for(unsigned int i = suite_length+1, j=0; i < path_length-1; i++,j++){	//path_length -1 to remove the  \n ... Suitelength+1 to remove the '_'
			name[j] = path[i];
		}

		if(testfw_register_symb(fw, suite, name)){
			sum +=1;
		}

	}
	pclose(f);
	free(cmd);
	return sum;
}

/* ********** RUN TEST **********	*/


pid_t child_pid = -1;

/**
 * @brief run all registered tests
 *
 * @param fw the test framework
 * @param argc the number of arguments passed to each test function
 * @param argv the array of arguments passed to each test function
 * @param mode the execution mode in which to run each test function
 * @return the number of tests that fail
 */
void alarm_handler(int sig) {	//Called when the parent's alarm is triggered
	kill(child_pid, SIGUSR1);		//Kill the child process with a custom signal
}

void print_log(struct testfw_t * fw, int status, int test_id, double exec_time, int fd){
		switch(status){
			case 0:		//Test success
				dprintf(fd, "[SUCCESS] run test \"%s.%s\" in %f ms (status %d)\n",fw->tests[test_id]->suite, fw->tests[test_id]->name, exec_time, WEXITSTATUS(status));
				return;
			case 256:	//Test failure
				dprintf(fd, "[FAILURE] run test \"%s.%s\" in %f ms (status %d)\n",fw->tests[test_id]->suite, fw->tests[test_id]->name, exec_time, WEXITSTATUS(status));
				return;
			case SIGUSR1:	//Timeout
				dprintf(fd, "[TIMEOUT] run test \"%s.%s\" in %f ms (status 124)\n", fw->tests[test_id]->suite, fw->tests[test_id]->name, exec_time);
				return;
			default:	//Any wrong thing append
				dprintf(fd, "[KILLED] run test \"%s.%s\" in %f ms (signal \"%s\")\n", fw->tests[test_id]->suite, fw->tests[test_id]->name, exec_time, strsignal(status));
				return;
		}
}

int testfw_run_all(struct testfw_t *fw, int argc, char *argv[], enum testfw_mode_t mode){
  if(!fw || !argv){
    fprintf(stderr, "Null pointer arguments\n");
    return EXIT_FAILURE;
  }

	int status = 0;

	int stdout_saved = dup(STDOUT_FILENO);
	int stderr_saved = dup(STDERR_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	int logfile_fileDescriptor;
	bool cmd_mode = false, logfile_mode = false;

	//If the logfile mode is enabled
	if(fw->logfile != NULL){
		logfile_mode = true;
		logfile_fileDescriptor = open(fw->logfile, O_CREAT | O_TRUNC | O_WRONLY, 0644);	//Create a new empty file
	}

	//If the cmd mode is enabled, for later simplicity
	if (fw->cmd != NULL){
		cmd_mode = true;
	}

	unsigned nb_failed_tests = 0;

	for(unsigned int i = 0; i < fw->nb_tests; i++) {
		child_pid = fork();

		if(child_pid == 0){	//Child
			if(!fw->verbose){
				close(stdout_saved);
				close(stderr_saved);
				close(STDOUT_FILENO);
				close(STDERR_FILENO);
			}

			if(fw->verbose){
				if(logfile_mode){
					dup2(logfile_fileDescriptor, STDOUT_FILENO);
					dup2(logfile_fileDescriptor, STDERR_FILENO);
				} else {
					close(logfile_fileDescriptor);
					dup2(stdout_saved, STDOUT_FILENO);
					dup2(stderr_saved, STDERR_FILENO);
				}
			}

			if(fw->silent){
				close(stdout_saved);
				close(stderr_saved);
				close(logfile_fileDescriptor);
			}


			exit(fw->tests[i]->func(argc,argv));

		} else {	//Main 'parent'

			/*	Timeout alarm and time calc initialisation	*/
			signal(SIGALRM, alarm_handler);

			struct timeval begin, end;
			gettimeofday(&begin, NULL);

			alarm(fw->timeout);
			waitpid(child_pid, &status, WUNTRACED);	//Wait the death of the child, put the exit status in &status
			alarm(0);	//Remove the alarm

			gettimeofday(&end, NULL);
			double t = (double)(end.tv_usec - begin.tv_usec) / 1000 + (end.tv_sec - begin.tv_sec)*1000;	//Time conversion

			/**	Printing side and special cases **/
			if(logfile_mode){
				print_log(fw, status, i, t, logfile_fileDescriptor);
			} else if(cmd_mode){	//Else, the same in the cmd input
				FILE * cmd_file = popen(fw->cmd, "w");
				assert(cmd_file);

				int	cmd_fileDescriptor = fileno(cmd_file);
				print_log(fw, status, i, t,	cmd_fileDescriptor);	//Print every tests that has been found by the cmd

				status = WEXITSTATUS(pclose(cmd_file));

				switch(status){
					case 2:	//Value returned if success
						print_log(fw, 0, i, t,	stdout_saved);		//Tests string passed the cmd
						break;
					default://Others cases than success (ie. failure)
						print_log(fw, 256, i, t,	stdout_saved);	//Tests string failed to passed the cmd
						nb_failed_tests += 1;
						break;
				}

			}

			/*
				If logfile is null, then its not the -S mode
				Else, we check it

				-S mode is incompatible with the logfile and cmd one
				In fact, there are also handled somewhere else
			*/
			if(!logfile_mode && !cmd_mode){
				if(fw->logfile == NULL) {
					print_log(fw, status, i, t, stdout_saved);
				} else {
					if(!fw->silent || strcmp(fw->logfile,"/dev/null") != 0){
						print_log(fw, status, i, t, stdout_saved);
					}
				}
			}

			//Count the number of failure to return.
			//Cmd mode has his own calculation
			if(status != 0 && !cmd_mode){	// If status success
				nb_failed_tests += 1;
			}

		}
	}

	//Close every opened file descriptor and reset the std output
	dup2(stdout_saved, STDOUT_FILENO);
	dup2(stderr_saved, STDERR_FILENO);

	close(stdout_saved);
	close(stderr_saved);


	return nb_failed_tests;
}
