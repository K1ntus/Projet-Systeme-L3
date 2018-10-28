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

//make some mem test there plz
struct testfw_t *testfw_init(char *program, int timeout, char *logfile, char *cmd, bool silent, bool verbose) {
  struct testfw_t* res = (struct testfw_t*) malloc(sizeof(struct testfw_t));
  if(!res){
    fprintf(stderr, "Invalid memory allocation\n");
  }


	res->tests = (struct test_t **) malloc(sizeof(struct test_t *) *150);
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
		if(fw->tests[i] != NULL)
    	free(fw->tests[i]);
  }
  free(fw->tests);

  if(fw->cmd)
    free(fw->cmd);
  if(fw->logfile)
    free(fw->logfile);
  if(fw->program)
    free(fw->program);


  free(fw);
}

int testfw_length(struct testfw_t *fw)  {
  return fw->nb_tests	;
}

struct test_t *testfw_get(struct testfw_t *fw, int k) {
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


struct test_t *testfw_register_symb(struct testfw_t *fw, char *suite, char *name) {

  unsigned int name_length  = 0;
  unsigned int suite_length = 0;

	if(!suite || ! name || !fw)
		return NULL;

  suite_length = strlen(suite);
  name_length = strlen(name);

  char *test_name = malloc(sizeof(char) * (name_length + suite_length + 2));

  strcat(test_name, suite);
	strcat(test_name, "_");
  strcat(test_name, name);


  void * handle_sym = dlopen("./sample", RTLD_NOW);;
  void * (*func) (int argc, char*argv);

  dlerror();  //clear error code


  func = dlsym(handle_sym, test_name);


  if(!handle_sym){
    fputs (dlerror(), stderr);
    //exit(1);
  }
  /*
  if((error = dlerror()) != NULL){  //cause segfault when used from register suite idk why
    fputs (dlerror(), stderr);
    exit(1);
  }*/

  //dlclose(handle_sym);  //Had to keep the handler opened to let those functionion visible. Else SEGFAULT youhou :3
  return testfw_register_func(fw, suite, name, (testfw_func_t) func);
}

int testfw_register_suite(struct testfw_t *fw, char *suite) {
  /*
  char * cmd = malloc(sizeof(char) * (strlen(suite) + strlen(fw->program) + strlen("nm --defined-only ./ | cut -d ' ' -f 3 | grep \"\"")));
  strcat(cmd, "nm --defined-only ./");
  strcat(cmd,fw->program);
  strcat(cmd, " | cut -d ' ' -f 3 | grep \"");
  strcat(cmd,suite);
  strcat(cmd,"\"");
*/
  if(suite == NULL || fw == NULL)
		return 0;

  char * cmd = malloc(sizeof(char) * (strlen(suite) + strlen("nm --defined-only ./sample | cut -d ' ' -f 3 | grep \"\"")));
	if(cmd == NULL)
		return 0;

  strcat(cmd, "nm --defined-only ./sample | cut -d ' ' -f 3 | grep \"^");
  strcat(cmd,suite);
  strcat(cmd,"\"");

	FILE * f = popen(cmd,"r");
	assert(f);
	char path[1024];

  unsigned int sum          = 0;
  unsigned int name_length  = 0;
  unsigned int suite_length = strlen(suite);
  unsigned int path_length  = 0;

  while(fgets(path, sizeof(path) -1, f) != NULL){  //print the output line per line
    if(path != NULL)
      path_length = strlen(path);

    name_length = path_length - suite_length;
    char *name = (char*) malloc(sizeof(char) * name_length);

    for(unsigned int i = suite_length+1, j=0; j < name_length; i++,j++){
      name[j] = path[i];
    }

    testfw_register_symb(fw, suite, name);
    sum +=1;
  }

  pclose(f);

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
