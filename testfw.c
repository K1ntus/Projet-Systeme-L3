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
	struct test_t ** tests;
	int nb_tests;

	char * program;
	char * logfile;
	char * cmd;

	int timeout;

	bool silent;
	bool verbose;
};

/* ********** FRAMEWORK ********** */

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

	int res_program = asprintf(&res->program, "%s",program);
	int res_logfile = asprintf(&res->logfile, "%s",logfile);
	int res_cmd = asprintf(&res->cmd, "%s",cmd);

	if(res_program < 0){
		res->program = "\0";
	}
	if(res_logfile < 0){
		res->logfile = "\0";
	}
	if(res_cmd < 0){
		res->cmd = "\0";
	}

	if(timeout > 0)
		res->timeout = timeout;
	else
		res->timeout = 0;


	if (silent == true){
		res->silent = true;
		res->verbose = false;
	}else{
		res->silent = false;
		res->verbose = true;
	}


	return res;
}

void testfw_free(struct testfw_t *fw) {
	for(unsigned int i = 0; i < fw->nb_tests; i++){
		if(fw->tests[i]){
			free(fw->tests[i]->suite);
			free(fw->tests[i]->name);
			//free(fw->tests[i]->func);
			free(fw->tests[i]);
		}
	}
	if(fw->tests)
		free(fw->tests);

	if(strcmp(fw->cmd, "\0") != 0)
		if(fw->cmd)
			free(fw->cmd);
	if(strcmp(fw->logfile, "\0") != 0)
		if(fw->logfile)
			free(fw->logfile);

	free(fw->program);

	free(fw);
}

int testfw_length(struct testfw_t *fw)	{
	return fw->nb_tests	;
}

struct test_t *testfw_get(struct testfw_t *fw, int k) {
	if(k < fw->nb_tests && k >= 0)
		return fw->tests[k];
	else
		return NULL;
}

/* ********** REGISTER TEST ********** */

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


struct test_t *testfw_register_symb(struct testfw_t *fw, char *suite, char *name) {

	if(!suite || !name || !fw)
		return NULL;


	char *test_name = NULL;
	int testName_res = asprintf(&test_name, "%s_%s",suite,name);

	if(testName_res <0 || !test_name)
		return NULL;

	void * handle_sym = dlopen(fw->program, RTLD_NOW);
	void * (*func) (int argc, char*argv);
	//void * error;

	dlerror();	//clear error code
	func =	dlsym(handle_sym, test_name);	//Return null if no

	free(test_name);
	return testfw_register_func(fw, suite, name, (testfw_func_t) func);
}



int testfw_register_suite(struct testfw_t *fw, char *suite) {
	if(!suite || !fw || !fw->program)
		return 0;

	char * cmd = NULL;
	int cmd_res = asprintf(&cmd, "nm --defined-only %s | cut -d ' ' -f 3 | grep \"^%s_\"",fw->program, suite);
	if(cmd_res < 0 || !cmd)
		return NULL;

	FILE * f = popen(cmd, "r");
	assert(f);
	char path[1024];

	unsigned int sum					= 0;
	unsigned int suite_length = 0;
	if(suite)
		suite_length = strlen(suite);
	unsigned int path_length	= 0;

	while(fgets(path, sizeof(path) -1, f) != NULL){	//print the output line per line
		unsigned int name_length	= 0;
		if(path != NULL)
			path_length = strlen(path);

		name_length = path_length - suite_length -1;
		char * name = (char*) malloc(sizeof(char) * (name_length));		//memleak there
		assert(name);
		name[0] = "";

		for(unsigned int i = suite_length+1, j=0; i < path_length-1; i++,j++){
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

/* ********** RUN TEST **********

****** A utiliser ******
strsignal()
gettimeofday()
popen()/pclose()
setjmp / longjmp
alarm(124)
*/

pid_t child_pid = -1;

void alarm_handler(void) {
	kill(child_pid, SIGTERM);
}

void print_log(struct testfw_t * fw, int status, int test_id, double exec_time){
	if(WIFSIGNALED(status)){
		if(status == 14)
			fprintf(stdout, "[KILLED] ");
		else if(exec_time >= fw->timeout)
			fprintf(stdout, "[TIMEOUT] ");
		else
			fprintf(stdout, "[KILLED] ");

		if(status == 14) { //Alarm clock value
			fprintf(stdout, "run test \"%s.%s\" in %f ms (status 124)\n", fw->tests[test_id]->suite, fw->tests[test_id]->name);
		}else{
			fprintf(stdout, "run test \"%s.%s\" in %f ms (signal \"%s\")\n", fw->tests[test_id]->suite, fw->tests[test_id]->name, exec_time, strsignal(status));
		}

	}	else {
		if(status == 0)
			fprintf(stdout, "[SUCCESS] ");
		else
			fprintf(stdout, "[FAILURE] ");
		fprintf(stdout, "run test \"%s.%s\" in %f ms (status %d)\n",fw->tests[test_id]->suite, fw->tests[test_id]->name, exec_time, WEXITSTATUS(status));
	}
}

int testfw_run_all(struct testfw_t *fw, int argc, char *argv[], enum testfw_mode_t mode){
  if(!fw || !argv){
    fprintf(stderr, "Null pointer arguments\n");
    return EXIT_FAILURE;
  }


	int saved = dup(1);	//Contains a copy to stdout
	int status = 0;

	//dup2(out_file, 1);
	//close(stdout);

	unsigned nb_failed_tests = 0;
	printf("bool silent : %d\n", fw->silent);

	if(fw->silent == false){
		for(unsigned int i = 0; i < fw->nb_tests; i++) {

			child_pid = fork();

			if(child_pid == 0){	//Child
				close(STDERR_FILENO);
				close(STDOUT_FILENO);

				exit(fw->tests[i]->func(argc,argv));

			} else {	//Main 'parent'
				signal(SIGALRM, alarm_handler);

				struct timeval begin, end;
				gettimeofday(&begin, NULL);

				alarm(fw->timeout);
				waitpid(child_pid, &status, WUNTRACED);
				alarm(0);

				gettimeofday(&end, NULL);
				double t = (double)(end.tv_usec - begin.tv_usec) / 1000 + (end.tv_sec - begin.tv_sec)*1000;
				print_log(fw, status, i, t);

				/*
				if(fw->logfile){	//A log file has been pointed out
					int logFile = open(fw->logfile, O_CREAT | O_TRUNC | O_WRONLY, 0644);
					dup2(logFile, STDOUT_FILENO);
					print_log(fw, status, i, t);
				}
				*/


				if(status != 0){
					nb_failed_tests += 1;
				}

				status = WEXITSTATUS(status);
			}
		}
		dup2(saved, 1);
	}else{
		int fd = open("logfile.txt", O_CREAT | O_TRUNC | O_WRONLY, 0644);
		if(!fd){
			exit(EXIT_FAILURE);
		}
		dup2(fd, STDOUT_FILENO);
		close(STDOUT_FILENO);
		for(unsigned int i = 0; i < fw->nb_tests; i++) {

			child_pid = fork();

			if(child_pid == 0){	//Child
				close(STDERR_FILENO);
				close(STDOUT_FILENO);

				exit(fw->tests[i]->func(argc,argv));

			} else {	//Main 'parent'
				signal(SIGALRM, alarm_handler);

				struct timeval begin, end;
				gettimeofday(&begin, NULL);

				alarm(fw->timeout);
				waitpid(child_pid, &status, WUNTRACED);
				alarm(0);

				gettimeofday(&end, NULL);
				double t = (double)(end.tv_usec - begin.tv_usec) / 1000 + (end.tv_sec - begin.tv_sec)*1000;

				print_log(fw, status, i, t);

				if(status != 0){
					nb_failed_tests += 1;
				}

				status = WEXITSTATUS(status);
			}
		}
		close(fd);
		dup2(saved, STDOUT_FILENO);
	}
	return nb_failed_tests;
}
