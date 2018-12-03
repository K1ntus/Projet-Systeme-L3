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

//STRLEN ON NULL => segfault, so Im using null character to manage those case
	res->program = (char *) malloc(sizeof(char) * (strlen(program) + 1));

	if(logfile != NULL){
		res->logfile = (char *) malloc(sizeof(char) * (strlen(logfile) + 1));
	}else{
		//fprintf(stderr, "Logfile value is null\n");
		res->logfile = "\0";
	}

	if(cmd != NULL){
		res->cmd = (char *) malloc(sizeof(char) * (strlen(logfile) + 1));
	}else{
		//fprintf(stderr, "cmd value is null\n");
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


	if(strcmp(res->logfile, "\0") != 0)
		strcpy(res->logfile, logfile);
	if(strcmp(res->cmd, "\0") != 0)
		strcpy(res->cmd, cmd);
	strcpy(res->program, program);

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
	char * suite1 = (char *) malloc(sizeof(suite));
	char * name1 = (char *) malloc(sizeof(name));
	testfw_func_t * fun = malloc(sizeof(func));

	*fun = func;
	if(suite != NULL)
		strcpy(suite1,suite);
	else
		suite1 = "\0";
	if(name != NULL)
		strcpy(name1, name);
	else
		name1="\0";

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
	unsigned int name_length	= 0;
	unsigned int suite_length = 0;

	if(!suite || ! name || !fw)
		return NULL;

	suite_length = strlen(suite);
	name_length = strlen(name);

	char *test_name = malloc(sizeof(char) * (name_length + suite_length + 2));
	strcpy(test_name, suite);
	strcat(test_name, "_");
	strcat(test_name, name);

	void * handle_sym = dlopen(fw->program, RTLD_NOW);
	void * (*func) (int argc, char*argv);
	//void * error;

	dlerror();	//clear error code
	func =	dlsym(handle_sym, test_name);	//Return null if no

	free(test_name);
	return testfw_register_func(fw, suite, name, (testfw_func_t) func);
}

int testfw_register_suite(struct testfw_t *fw, char *suite) {
	if(!suite || !fw)
		return 0;

//Utiliser asprintf
	char * cmd = malloc(sizeof(char) * (strlen(suite) + strlen(fw->program) + strlen("nm --defined-only ./ | cut -d ' ' -f 3 | grep \"\"")));
	assert(cmd);

	strcpy(cmd, "nm --defined-only ");
	strcat(cmd, fw->program);
	strcat(cmd, " | cut -d ' ' -f 3 | grep \"^");
	strcat(cmd, suite);
	strcat(cmd,"_\"");

	FILE * f = popen(cmd,"r");
	assert(f);
	char path[1024];

	unsigned int sum					= 0;
	unsigned int suite_length = strlen(suite);
	unsigned int path_length	= 0;

	while(fgets(path, sizeof(path) -1, f) != NULL){	//print the output line per line
		unsigned int name_length	= 0;
		if(path != NULL)
			path_length = strlen(path);

		name_length = path_length - suite_length -1;
		char * name = (char*) malloc(sizeof(char) * (name_length));		//memleak there
		assert(name);

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

void print_log(struct testfw_t * fw, int status, int test_id,double exec_time){
	if(WIFSIGNALED(status)){
		if(exec_time >= fw->timeout)
			printf("[TIMEOUT] ");
		else
			printf("[KILLED] ");

		if(status == 14) { //Alarm clock value
			printf("run test \"%s.%s\" in %f ms (status 124)\n", fw->tests[test_id]->suite, fw->tests[test_id]->name, exec_time);
		}else{
			printf("run test \"%s.%s\" in %f ms (signal \"%s\")\n", fw->tests[test_id]->suite, fw->tests[test_id]->name, exec_time, strsignal(status));
		}

	}	else {
		if(status == 0)
			printf("[SUCCESS] ");
		else
			printf("[FAILURE] ");
		printf("run test \"%s.%s\" in %f ms (status %d)\n",fw->tests[test_id]->suite, fw->tests[test_id]->name, exec_time, WEXITSTATUS(status));
	}
}

int testfw_run_all(struct testfw_t *fw, int argc, char *argv[], enum testfw_mode_t mode){
  if(!fw || !argv){
    fprintf(stderr, "Null pointer arguments\n");
    return EXIT_FAILURE;
  }


	int saved = dup(1);	//Contains a copy to stdout
	int stdout = 1;
	int stderr = 2;
	int status = 0;

	close(stderr);
	close(stdout);
	//dup2(out_file, 1);
	//close(stdout);


		//dup2(fd, 1);
	unsigned nb_failed_tests = 0;

	for(unsigned int i = 0; i < fw->nb_tests; i++) {

		child_pid = fork();

		if(child_pid == 0){	//Child
			exit(fw->tests[i]->func(argc,argv));

		} else {	//Main 'parent'
			signal(SIGALRM,alarm_handler);

			struct timeval begin, end;
			gettimeofday(&begin, NULL);

			alarm(fw->timeout);
			waitpid(child_pid, &status, WUNTRACED);
			alarm(0);

			gettimeofday(&end, NULL);
			double t = (double)(end.tv_usec - begin.tv_usec) / 1000 + (double)(end.tv_sec - begin.tv_sec);

			print_log(fw, status, i, t);

			if((status) != 0){
				nb_failed_tests += 1;
			}

			status = WEXITSTATUS(status);


		}
	}
	dup2(saved, 1);
	return nb_failed_tests;
}
