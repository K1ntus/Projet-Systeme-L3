#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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
struct testfw_t
{
	struct test_t ** tests;
	int nb_tests;

	char * program;
	char * logFile;
	char * cmd;

	int timeout;

	bool silent;
	bool verbose;
};

/* ********** FRAMEWORK ********** */

struct testfw_t *testfw_init(char *program, int timeout, char *logfile, char *cmd, bool silent, bool verbose)
{
  struct testfw_t* res = (struct testfw_t*) malloc(sizeof(struct testfw_t*) + 2*sizeof(int) + 3*sizeof(char*) + 2*sizeof(bool) + sizeof(struct test_t));
  if(!res){
    fprintf(stderr, "Invalid memory allocation\n");
  }


	res->tests = (struct test_t **) malloc(sizeof(struct test_t **)+sizeof(struct test_t *) *150);
	if(!res->tests){
	  fprintf(stderr, "Invalid memory allocation to save the tests data\n");
	}
	res->nb_tests = 0;

  res->program = (char *) malloc(sizeof(char *));
  if(!res->program){
    fprintf(stderr, "Invalid memory allocation to save the program executable name\n");
  }

  res->logFile = (char *) malloc(sizeof(char *));
  if(!res->logFile){
    fprintf(stderr, "Invalid memory allocation to save the logFile name\n");
  }

  res->cmd = (char *) malloc(sizeof(char *));
  if(!res->cmd){
    fprintf(stderr, "Invalid memory allocation to save the cmd\n");
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


    res->program = program;
    res->logFile = logfile;
    res->cmd = cmd;

		return res;
}

void testfw_free(struct testfw_t *fw)
{
	//Add memory tests
  //	free(fw->tests);
  free(fw->program);
  free(fw->logFile);
  free(fw->cmd);
  free(fw);
}

int testfw_length(struct testfw_t *fw)
{

  return fw->nb_tests	;
}

struct test_t *testfw_get(struct testfw_t *fw, int k)
{
    return fw->tests[k];
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

struct test_t *testfw_register_symb(struct testfw_t *fw, char *suite, char *name)
{
    return NULL;
}

int testfw_register_suite(struct testfw_t *fw, char *suite)
{
    return EXIT_SUCCESS;
}

/* ********** RUN TEST ********** */

int testfw_run_all(struct testfw_t *fw, int argc, char *argv[], enum testfw_mode_t mode)
{
	for(unsigned int i = 0; i < fw->nb_tests; i++){
		fw->tests[i]->func(argc, argv);
	}
  return EXIT_SUCCESS;
}
