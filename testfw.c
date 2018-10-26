#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <dlfcn.h>

#include "testfw.h"

/* ********** STRUCTURES ********** */

/**
 * @brief test structure
 */
 /*
struct test_t
{
    char *suite;        	//< suite name
    char *name;         	//< test name
    testfw_func_t func; 	//< test function
};
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
  struct testfw_t* res = (struct testfw_t*) malloc(sizeof(struct testfw_t));
  if(!res){
    fprintf(stderr, "Invalid memory allocation\n");
  }


	res->tests = (struct test_t **) malloc(sizeof(struct test_t **)+sizeof(struct test_t *) *150);
	if(!res->tests){
	  fprintf(stderr, "Invalid memory allocation to save the tests data\n");
	}
  
	res->nb_tests = 0;


//STRLEN ON NULL => segfault
  if(program != NULL)
    res->program = (char *) malloc(sizeof(char) * (strlen(program) + 1));
  else
    res->program = NULL;


  if(logfile != NULL)
    res->logfile = (char *) malloc(sizeof(char) * (strlen(logfile) + 1));
  else
    res->logfile = NULL;


  if(cmd != NULL)
    res->cmd = (char *) malloc(sizeof(char) * (strlen(cmd) + 1));
  else
    res->cmd = NULL;


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


    if(res->logfile)
      strcpy(res->logfile, logfile);
    if(res->logfile)
      strcpy(res->logfile, logfile);
    if(res->cmd)
      strcpy(res->cmd, cmd);

		return res;
}

void testfw_free(struct testfw_t *fw) {
  for(unsigned int i = fw->nb_tests; i > 0; i--){
    free(fw->tests[i]);
  }
  free(fw->tests);

  if(fw->cmd)
    free(fw->cmd);

  free(fw->program);  
  free(fw->logfile);  
  
  free(fw);
}

int testfw_length(struct testfw_t *fw)  {

  return fw->nb_tests	;
}

struct test_t *testfw_get(struct testfw_t *fw, int k) {
  return NULL;
  /*
  return fw->tests[k];
  */
}

/* ********** REGISTER TEST ********** */

struct test_t *testfw_register_func(struct testfw_t *fw, char *suite, char *name, testfw_func_t func)	{
    struct test_t *res = (struct test_t *) malloc(sizeof(struct test_t));
		res -> suite = suite;
		res -> name = name;
		res -> func = func;

		//realloc fw->tests
		fw->tests[fw->nb_tests] = res;
		fw->nb_tests += 1;

		return res;
}


struct test_t *testfw_register_symb(struct testfw_t *fw, char *suite, char *name) {
  char *test_name = malloc(sizeof(char) * (strlen(suite) + strlen(name) + 1));
  strcat(test_name, suite);
  strcat(test_name, "_");
  strcat(test_name, name);


  void * handle_sym;
  void * (*func) (int argc, char*argv);
  char * error;

  dlerror();  //clear error code

  handle_sym = dlopen("./sample", RTLD_NOW);
  if(!handle_sym){
    fputs (dlerror(), stderr);
    exit(1);
  }

  func = dlsym(handle_sym, test_name);
  if((error = dlerror()) != NULL){
    fputs (dlerror(), stderr);
    exit(1);
  }

  //dlclose(handle_sym);  //Had to keep the handler opened to let those functionion visible. Else segfault :3
  return testfw_register_func(fw, suite, name, (testfw_func_t) func);
}

int testfw_register_suite(struct testfw_t *fw, char *suite)
{
    return EXIT_SUCCESS;
}

/* ********** RUN TEST ********** */

int testfw_run_all(struct testfw_t *fw, int argc, char *argv[], enum testfw_mode_t mode)
{
	/*for(unsigned int i = 0; i < fw->nb_tests; i++){
		fw->tests[i]->func(argc, argv);
	}*/
  return EXIT_SUCCESS;
}
