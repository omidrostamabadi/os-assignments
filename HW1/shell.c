#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80
#define MAX_PATH_SIZE 300
#define MAX_FILE_NAME 50

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

/* some helper functions */

void get_full_path(const char *file, char *full_path);

int cmd_quit(tok_t arg[]) {
  printf("Bye\n");
  exit(0);
  return 1;
}

int cmd_help(tok_t arg[]);

int cmd_pwd(tok_t arg[]);

int cmd_chdir(tok_t arg[]);


/* Command Lookup table */
typedef int cmd_fun_t (tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_quit, "quit", "quit the command shell"},
  {cmd_pwd, "pwd", "print working directory"},
  {cmd_chdir, "chdir", "change working directory"},
};

int cmd_help(tok_t arg[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    printf("%s - %s\n",cmd_table[i].cmd, cmd_table[i].doc);
  }
  return 1;
}

int cmd_pwd(tok_t arg[]) {
  char *abs_path = (char*)malloc(MAX_PATH_SIZE * sizeof(char));
  abs_path = getcwd(abs_path, MAX_PATH_SIZE);
  printf("%s\n", abs_path);
  free(abs_path);
  return 0;
}

int cmd_chdir(tok_t arg[]) {
  int result = chdir(arg[0]);
  if(result != 0) {
    fprintf(stderr, "chdir failed, %s is not a valid directory\n", arg[0]);
  }
  return result;
}

int lookup(char cmd[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
  }
  return -1;
}

void init_shell()
{
  /* Check if we are running interactively */
  shell_terminal = STDIN_FILENO;

  /** Note that we cannot take control of the terminal if the shell
      is not interactive */
  shell_is_interactive = isatty(shell_terminal);

  if(shell_is_interactive){

    /* force into foreground */
    while(tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp()))
      kill( - shell_pgid, SIGTTIN);

    shell_pgid = getpid();
    /* Put shell in its own process group */
    if(setpgid(shell_pgid, shell_pgid) < 0){
      perror("Couldn't put the shell in its own process group");
      exit(1);
    }

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    tcgetattr(shell_terminal, &shell_tmodes);
  }
  /** YOUR CODE HERE */
}

/**
 * Add a process to our process list
 */
void add_process(process* p)
{
  /** YOUR CODE HERE */
}

/**
 * Creates a process given the inputString from stdin
 */
process* create_process(char* inputString)
{
  /** YOUR CODE HERE */
  return NULL;
}



int shell (int argc, char *argv[]) {
  char *s = malloc(INPUT_STRING_SIZE+1);			/* user input string */
  tok_t *t;			/* tokens parsed from input */
  int lineNum = 0;
  int fundex = -1;
  pid_t pid = getpid();		/* get current processes PID */
  pid_t ppid = getppid();	/* get parents PID */
  pid_t cpid, tcpid, cpgid;

  init_shell();

  // printf("%s running as PID %d under %d\n",argv[0],pid,ppid);

  lineNum=0;
  // fprintf(stdout, "%d: ", lineNum);
  while ((s = freadln(stdin))){
    t = getToks(s); /* break the line into tokens */
    fundex = lookup(t[0]); /* Is first token a shell literal */
    if(fundex >= 0) cmd_table[fundex].fun(&t[1]);
    else {
      //fprintf(stdout, "This shell only supports built-ins. Replace this to run programs as commands.\n");
      char *full_path = (char*)malloc(MAX_PATH_SIZE);
      get_full_path(t[0], full_path);
      if(strcmp(full_path, "/") == 0) { // could not find the file
        printf("file %s not found\n", t[0]);
        continue;
      }
      pid_t pid = fork();
      if(pid == 0) { // child process executes program
        int result = execv(full_path, &t[0]); // returns only if error
        fprintf(stderr, "Failed to run program %s\n", t[0]);
        exit(0);
      }
      else { // parent waits for the child
        int stat_loc;
        int options = 0; // no flags
        waitpid(pid, &stat_loc, options);
      }
    }
    // fprintf(stdout, "%d: ", lineNum);
  }
  return 0;
}

/* some helper functions */

/* 
* get the path of a program to execute
* if it is the full path, return
* else search every directory in PATH envvar for the program
* if the full_path is '/', indicates that file has not been found
*/
void get_full_path(const char *file, char *full_path) {
  // if file contains '/' in it, it is a full path
  if(strchr(file, '/') != NULL) {
    strcpy(full_path, file);
    return;
  }

  // add a '/' to the beginning of the file
  // use this aug_file name to concat path options
  char *aug_file = (char*)malloc(MAX_FILE_NAME * sizeof(char));
  strcpy(aug_file, "/");
  aug_file = strcat(aug_file, file);

  // get the PATH envvar and tokenize it into paths
  const char *path_env;
  path_env = getenv("PATH");
  char *tmp_path = (char*)malloc(MAX_PATH_SIZE * sizeof(char));
  strcpy(tmp_path, path_env);
  tok_t *paths = getToks(tmp_path);

  // check all the paths for file
  // return on first match
  int path_idx = 0;
  int found = FALSE;
  full_path = strcpy(full_path, paths[path_idx]);
  while(paths[path_idx] != NULL) { // try all paths
    full_path = strcpy(full_path, paths[path_idx]);
    full_path = strcat(full_path, aug_file);
    if(access(full_path, F_OK) == 0) {
      printf("Found at %s\n", full_path);
      found = TRUE;
      break;
    }
    path_idx++;
  }
  free(tmp_path);
  if(found == FALSE) { // not found the file
    strcpy(full_path, "/");
  } 
}