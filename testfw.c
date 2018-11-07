#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <dlfcn.h>

#include "testfw.h"

/* ********** STRUCTURES ********** */

/**
 * @brief test structure
 */
struct testfw_t  {
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
	for(unsigned int i = 0; i < fw->nb_tests; i++)
	  if(fw->tests[i])
			free(fw->tests[i]);
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

int testfw_length(struct testfw_t *fw)  {
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
		res -> suite = suite;
		res -> name = name;
		res -> func = func;

		struct test_t **tmp = (struct test_t**)realloc(fw->tests, (fw->nb_tests+1)*sizeof(struct test_t*));
		if (tmp != NULL) {
		    fw->tests = tmp;
		} else {
		    // Do something about the failed allocation
		}

		fw->tests[fw->nb_tests] = res;
		fw->nb_tests += 1;
		return res;
}


struct test_t *testfw_register_symb(struct testfw_t *fw, char *suite, char *name) {

  unsigned int name_length  = 0;
  unsigned int suite_length = 0;

	if(!suite || !name || !fw)
		return NULL;

  suite_length = strlen(suite);
  name_length = strlen(name);

  char *test_name = malloc(sizeof(char) * (name_length + suite_length + 3));
  strcpy(test_name, suite);
	strcat(test_name, "_");
  strcat(test_name, name);
  strcat(test_name, "\n");

  void * handle_sym = dlopen(fw->program, RTLD_NOW);;
  void * (*func) (int argc, char*argv);
	void * error;

  dlerror();  //clear error code

  func =  dlsym(handle_sym, test_name);	//Return null if no equivalence found in this file

	if((error = dlerror()) == NULL){
    //fputs (dlerror(), stderr);
		dlclose(handle_sym);
		free(test_name);
		return NULL;
	}

	free(test_name);
	//printf("SUITE:%s, NAME:%s\n",suite,name);

  //dlclose(handle_sym);  //Had to keep the handler opened to let those function visible for the program. Else SEGFAULT youhouu
  return testfw_register_func(fw, suite, name, (testfw_func_t) func);
}

int testfw_register_suite(struct testfw_t *fw, char *suite) {
  if(!suite || !fw)
		return 0;

	char * cmd = malloc(sizeof(char) * (strlen(suite) + strlen(fw->program) + strlen("nm --defined-only ./ | cut -d ' ' -f 3 | grep \"\"")));
	assert(cmd);

	strcpy(cmd, "nm --defined-only ");
	strcat(cmd,fw->program);
	strcat(cmd, " | cut -d ' ' -f 3 | grep \"^");
	strcat(cmd,suite);
	strcat(cmd,"\"");

	FILE * f = popen(cmd,"r");
	assert(f);
	char path[1024];

  unsigned int sum          = 0;
  unsigned int suite_length = strlen(suite);
  unsigned int path_length  = 0;

  while(fgets(path, sizeof(path) -1, f) != NULL){  //print the output line per line
  	unsigned int name_length  = 0;

    if(path != NULL)
      path_length = strlen(path);

    name_length = path_length - suite_length;
    char * name = (char*) malloc(sizeof(char) * (name_length+1));		//memleak there
		assert(name);

    for(unsigned int i = suite_length+1, j=0; j < name_length; i++,j++){
      name[j] = path[i];
    }

    testfw_register_symb(fw, suite, name);
    sum +=1;

  }

  pclose(f);
	free(cmd);

	return sum;
}

/* ********** RUN TEST ********** */

int testfw_run_all(struct testfw_t *fw, int argc, char *argv[], enum testfw_mode_t mode)
{
	/*for(unsigned int i = 0; i < fw->nb_tests; i++){
		fw->tests[i]->func(argc, argv);
	}*/
  return EXIT_SUCCESS;
}
