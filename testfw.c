#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>

#include "testfw.h"

/* ********** STRUCTURES ********** */

struct testfw_t
{
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
  struct testfw_t* res = (struct testfw_t*) malloc(sizeof(struct testfw_t*));
  if(!res){
    fprintf(stderr, "Invalid memory allocation\n");
  }

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
}

void testfw_free(struct testfw_t *fw)
{
	//Add memory tests
  free(fw->program);
  free(fw->logFile);
  free(fw->cmd);
  free(fw);
}

int testfw_length(struct testfw_t *fw)
{
  unsigned int sum = 0;

  return sum;
}

struct test_t *testfw_get(struct testfw_t *fw, int k)
{
    return NULL;
}

/* ********** REGISTER TEST ********** */

struct test_t *testfw_register_func(struct testfw_t *fw, char *suite, char *name, testfw_func_t func)
{
    return NULL;
}

struct test_t *testfw_register_symb(struct testfw_t *fw, char *suite, char *name)
{
    return NULL;
}

int testfw_register_suite(struct testfw_t *fw, char *suite)
{
    return 0;
}

/* ********** RUN TEST ********** */

int testfw_run_all(struct testfw_t *fw, int argc, char *argv[], enum testfw_mode_t mode)
{
    return 0;
}
