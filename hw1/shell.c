#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80
#define MAX_PATH_SIZE 300
#define MAX_FILE_NAME 50
#define MAX_CHILDS 50
#define R TRUE
#define T FALSE

//#define DBG

#ifdef DBG
  #define PRINT(str) printf(str)
#else
  #define PRINT(str) while(0)
#endif

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"

/* 
* this structure holds info about redirection
* keeps two flags for input/output redirection
* also stores two file names for each redirection
*/
typedef struct redirection_info{
  char *input_file; // input file to redirect from
  char *output_file; // output file to redirect to
  int in_redir; // if input is redirected
  int out_redir; // if output is redirected
} redir_info_t;

/*
* hold information about child processes
* this is currently used in wait built-in 
* want to wait for running children only
*/
typedef struct child_info{
  pid_t pid; // pid of the child process
  pid_t pgid; // pgid of child process
  int status; // R (running) - T (stopped) (see #defines above)
  struct child_info *next; // next child in the list
} child_t;

/* a linked-list to manage children */
child_t *children = NULL;

/* some helper functions */

void get_full_path(const char *file, char *full_path);
void get_redirection_info(tok_t *toks, redir_info_t *rinfo);
int check_background(tok_t *toks);
void child_handler(int sig, siginfo_t *sig_info, void *void_var);
void put_child_in_list(child_t *child);
void remove_child_from_list(pid_t pid);
void change_child_status(pid_t pid, int new_status);

int cmd_quit(tok_t arg[]) {
  printf("Bye\n");
  exit(0);
  return 1;
}

int cmd_help(tok_t arg[]);

int cmd_pwd(tok_t arg[]);

int cmd_cd(tok_t arg[]);

int cmd_wait(tok_t arg[]);




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
  {cmd_cd, "cd", "change working directory"},
  {cmd_wait, "wait", "wait for all background processes to finish"},
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

int cmd_cd(tok_t arg[]) {
  int result = chdir(arg[0]);
  if(result != 0) {
    fprintf(stderr, "cd failed, %s is not a valid directory\n", arg[0]);
  }
  return result;
}

int cmd_wait(tok_t arg[]) {
  int stat_loc;
  int options = WUNTRACED;
  pid_t running_children[MAX_CHILDS];
  pid_t curr_child;
  PRINT("Just before\n");
  child_t *list = children;
  /* wait for all running children to finish */
  while(list != NULL) {
    if(list->status == R)
      waitpid(list->pid, NULL, options);
    list = list->next;
    PRINT("Wait for next child\n");
  }
  if(children == NULL)
    PRINT("children is null!\n");
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
  struct sigaction act;
  sigemptyset(&act.sa_mask);
  /* if SA_RESTART is not set, causes weird behaviour when calling freadln
  returns EOF from input */
  act.sa_flags = SA_SIGINFO | SA_RESTART; /* use sa_sigaction (with 3 args instead of 1) */
  //act.sa_flags = 0;
  act.sa_sigaction = child_handler;
  sigaction(SIGCHLD, &act, NULL);
  int run_in_background = FALSE;
  /* the shell ignores this signals */
  signal(SIGTTOU, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  while ((s = freadln(stdin))){
    PRINT("Input:\n");
    PRINT(s);
    /* after entering signals (like ^C) we receive empty input
    processing it may lead to SIGSEGV
    so skip empty inputs */
    if(!strcmp(s, "\n")) {
    //  printf("Empty input\n");
      continue;
    }
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

      /* check if run in background */
      run_in_background = check_background(t);
      /* check input/output redirection */
      redir_info_t redir_info;
      get_redirection_info(t, &redir_info);
      int stdin_fd = dup(STDIN_FILENO); // to be restored later
      int stdout_fd = dup(STDOUT_FILENO); // to be restored later


      pid_t pid = fork();
      if(pid == 0) { // child process executes program
        /* child ingerits parent signals if ignored
        restore signal handling to default for the child */
        signal(SIGTSTP, SIG_DFL); // child should not ignore SIGTSTP
        signal(SIGINT, SIG_DFL); // child should not ignore SIGINT
        signal(SIGQUIT, SIG_DFL); // child should not ignore SIGQUIT
        /* check input redirection */
        int fid_redir_in;
        if(redir_info.in_redir == TRUE) { // if input is redirected
          fid_redir_in = open(redir_info.input_file, O_RDONLY);
          int ret_val = dup2(fid_redir_in, STDIN_FILENO);
          if(ret_val < 0) {
          //  printf("Could not read from file %s\n", redir_info.input_file);
            continue;
          }
        }
        /* check output redirection */
        int fid_redir_out;
        if(redir_info.out_redir == TRUE) { // if output is redirected
            fid_redir_out = open(redir_info.output_file, O_RDWR | O_CREAT, 
            S_IWGRP | S_IRGRP | S_IRUSR | S_IWUSR | S_IROTH);
            int ret_val = dup2(fid_redir_out, STDOUT_FILENO);
            if(ret_val < 0) {
            //  printf("Could not open file %s for output\n", redir_info.output_file);
              continue;
            }
          }
        /* set pgid of this process to its pid (leader of the group) */
        pid_t my_pid = getpid();
        setpgid(0, my_pid); // set this process as the head of a process group
        pid_t pgid = getpgrp();
        if(run_in_background == FALSE) { // put into foreground
          int val_ret = tcsetpgrp(shell_terminal, pgid);
        }
        else { // should not ignore TTOU and TTIN if in background
          signal(SIGTTIN, SIG_DFL);
          signal(SIGTTOU, SIG_DFL);
        }
        
        int result = execv(full_path, &t[0]); // returns only if error
       // fprintf(my_fd, "Failed to run program %s\n", t[0]);
        exit(1); // execv only returns on fail
      }
      else { // parent waits for the child
        /* add current child to the children list */
        child_t *this_child = (child_t*)malloc(sizeof(child_t));
        this_child->pid = pid;
        this_child->pgid = pid;
        this_child->status = R; // running
        this_child->next = NULL;
        put_child_in_list(this_child);

        if(run_in_background == FALSE) {
          int stat_loc;
          /* important: without WUNTRACED flag, we get blocked at 
          stopped children until they finish! */
          int options = WUNTRACED; /* also return if child has stopped */
          waitpid(pid, &stat_loc, options);
        }
        /* foreground process finished, or want to run
        a process in background
        so take control of the terminal */
        int val_ret = tcsetpgrp(shell_terminal, shell_pgid);
        int cntrl = tcgetpgrp(shell_terminal);
        PRINT("Parent leaves else...\n");
      }
    }
    // fprintf(stdout, "%d: ", lineNum);
  }
  if(s == EOF)
    PRINT("EOF in S\n");
  PRINT("s out = \n");
  PRINT(s);
  PRINT("Exiting while\n");
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
      //printf("Found at %s\n", full_path);
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

/*
* fill up rinfo struct with information on redirection
* specifiy if input/output are redirected
* if there is a redirection, set corresponding file names
*/
void get_redirection_info(tok_t *toks, redir_info_t *rinfo) {
  rinfo->in_redir = FALSE;
  rinfo->out_redir = FALSE;
  int tok_index = 0;
  while(toks[tok_index] != NULL) { // go over all tokens
    if(strcmp(toks[tok_index], ">") == 0) { // check output redirection
      rinfo->out_redir = TRUE;
      // assume file name immediately follows '>'
      rinfo->output_file = toks[tok_index + 1];
      toks[tok_index] = NULL;
      toks[tok_index + 1] = NULL;
    }
    else if(strcmp(toks[tok_index], "<") == 0) { // check input redirection
      rinfo->in_redir = TRUE;
      // assume file name immediately follows '<'
      rinfo->input_file = toks[tok_index + 1];
      toks[tok_index] = NULL;
      toks[tok_index + 1] = NULL;
    }
    tok_index++;
  }
}

/*
* check if a '&' is at the end of input
* return TRUE if '&' is found
* return FALSE otherwise
*/
int check_background(tok_t *toks) {
  int tok_index = 0;
  tok_t last_tok = NULL;
  /* traverse tokens to get the last token */
  while(toks[tok_index] != NULL) {
    last_tok = toks[tok_index];
    tok_index++;
  }
  if(!strcmp(last_tok, "&")) {
    toks[tok_index - 1] = NULL; // '&' shouldn't be passed to executable
    return TRUE; // should run process in background if '&' is at the end
  }
  else {
    return FALSE;
  }
}

/* put child into the children list */
void put_child_in_list(child_t *child) {
  PRINT("Putting child into list...\n");
  if(children == NULL) { // list is empty
    children = child;
    return;
  }
  /* traverse the list and puth child at the end */
  child_t *list = children;
  while(list->next != NULL) {
    list = list->next;
  }
  /* when reach here, list points to the last element */
  list->next = child;
}

/* remove child whose process id is equal to pid from the list */
void remove_child_from_list(pid_t pid) {
  PRINT("Removing child from list...\n");
  if(children == NULL)
    return;
  else if(children != NULL && children->next == NULL) { // only one child
    if(children->pid == pid) { // remove the only child
      free(children);
      children = NULL;
    }
    return;
  }
  /* if reach here, at least two childs available */
  child_t *prev = children;
  child_t *list = prev->next;
  while(list != NULL) {
    if(list->pid == pid) { // found the child
      prev->next = list->next; // remove this node
      free(list);
      return;
    }
    prev = list;
    list = list->next;
  }
}

/* change the status of child whose process id is pid to new_status */
void change_child_status(pid_t pid, int new_status) {
  PRINT("Changing child status in list...\n");
  if(children == NULL)
    return;
  child_t *list = children;
  while(list != NULL) {
    if(list->pid == pid) {
      list->status = new_status;
    }
    list = list->next;
  }
}

/* 
* handle SIGCHLD signal
* if child exited, remove it from the list
* if child stopped or continued, change its status
*/
void child_handler(int sig, siginfo_t *sig_info, void *void_var) {
  PRINT("Parent in child handler...\n");
  if(sig_info->si_code == CLD_EXITED) { // the child exited
    pid_t child_pid = sig_info->si_pid;
    remove_child_from_list(child_pid);
  }
  else if(sig_info->si_code == CLD_STOPPED) { // child received stop signal
    pid_t child_pid = sig_info->si_pid;
    change_child_status(child_pid, T);
  }
  else if(sig_info->si_code == CLD_CONTINUED) { // child continued
    pid_t child_pid = sig_info->si_pid;
    change_child_status(child_pid, R);
  }
//  fflush(stdin);
  PRINT("Return from handler\n");
}
