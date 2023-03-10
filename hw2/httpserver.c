#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>
#include <semaphore.h>

#include "libhttp.h"
#include "wq.h"

/* 
* Constants used in this file
*/
#define MAX_CONTENT_LENGTH_DIGITS 20
#define MAX_CON_LEN MAX_CONTENT_LENGTH_DIGITS
#define MAX_DIRENT_NAME_LEN 200
#define MAX_ENT_NUM 100
#define PROXY_BUFFER_SIZE 10000

/*
* Structs are defined here
*/

/* This struct is passed as an argument to up/downstream threads of handle_proxy_request */
struct proxy_socket {
  int fd;
  int cl_sock_fd;
  pthread_cond_t *up;
  pthread_cond_t *down;
  pthread_cond_t *status_wait;
  pthread_mutex_t *status_check;
  pthread_mutex_t *downmu;
  sem_t *done;
  int *finished;
};

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue;
int num_threads;
int server_port;
char *server_files_directory;
char *server_proxy_hostname;
int server_proxy_port;
pthread_t *threads;

/* Helper functions are defined here */

/*
* Create a html page to represent the children of path directory
* The caller must ensure path exits and is a directory
* The caller is responsible for freeing the returned char * pointer
* Does not list . and ..
*/
char *list_dirs(const char *path) {
  DIR *dp;
  struct dirent *dirp;
  dp = opendir(path);
  if(dp == NULL) { // Error opening directory
    switch(errno) {
      case EACCES: printf("Permission denied\n");
      case ENOENT: printf("Specified directory %s does not exist\n", path);
      case ENOTDIR: printf("%s is not a directory\n", path);
    }
    exit(EXIT_FAILURE);
  }
  char *tmp_ent_name = malloc(MAX_DIRENT_NAME_LEN);
  char *dir_list_html = malloc(MAX_DIRENT_NAME_LEN * MAX_ENT_NUM);

  /* First, put a reference to parent directory */
  sprintf(tmp_ent_name, "<a href=\"../\">Parent directory</a><br>\n");
  strcpy(dir_list_html, tmp_ent_name);

  /* Iterate over all enteries and put a reference to each */
  while((dirp = readdir(dp)) != NULL) {
    if(strcmp(".", dirp->d_name) && strcmp("..", dirp->d_name)) { // Skip . and ..
      if(dirp->d_type == 4) { // A directory, should add '/' to the end of the link
        sprintf(tmp_ent_name, "<a href=\"./%s/\">%s</a><br>\n", dirp->d_name, dirp->d_name);
        strcat(dir_list_html, tmp_ent_name);
      }
      else { // Not a directory, form links without an end '/'
        sprintf(tmp_ent_name, "<a href=\"./%s\">%s</a><br>\n", dirp->d_name, dirp->d_name);
        strcat(dir_list_html, tmp_ent_name);
      }
    }
  }

  /* Free allocated memory */
  free(tmp_ent_name);

  return dir_list_html;
}

/*
* Wait for data on local socket in proxy requests
* Writes information quickly to target http server
* Thread should terminate if the other thread has exited
*/
void *proxy_thread_handle_upstream(void *socks) {
  /* Initialize buffer & variables */
  char *buffer = malloc(PROXY_BUFFER_SIZE);
  ssize_t data_len;
  int write_status = 0;
  struct proxy_socket *my_sock = (struct proxy_socket*) socks;

  /* Read from fd and write to client_socket_fd */
  while(1) {
    data_len = recv(my_sock->fd, buffer, PROXY_BUFFER_SIZE - 1, 0);
    if(data_len <= 0) { // fd has been closed, break
      break;
    }
    else {
      write_status = http_send_data(my_sock->cl_sock_fd, buffer, data_len);
    }

    if(write_status < 0) { // write failed, cl_sock_fd has been closed
      break;
    }
  }

  /* Inform peer thread that we will not be writing anymore, so read in 
  down stream fails and exits the while loop */
  shutdown(my_sock->cl_sock_fd, SHUT_WR);

  /* We wait for peer thread to finish */
  pthread_mutex_lock(my_sock->status_check);
  if(*my_sock->finished == 0) {
    *(my_sock->finished) = 1;
    pthread_cond_wait(my_sock->status_wait, my_sock->status_check);
  }
  else if(*my_sock->finished == 1) {
    *(my_sock->finished) = 2;
    printf("Up signaling...\n");
    pthread_cond_signal(my_sock->status_wait);
  }
  pthread_mutex_unlock(my_sock->status_check);

  /* Infrom parent thread to close the sockets */
  sem_post(my_sock->done);

  /* Free resources and exit */
  free(buffer);
  return NULL;
}

/*
* Wait for data on local socket in proxy requests
* Writes information quickly to target http server
* Thread should terminate if the other thread has exited
*/
void *proxy_thread_handle_downstream(void *socks) {
  /* Initialize buffer & variables */
  char *buffer = malloc(PROXY_BUFFER_SIZE);
  ssize_t data_len;
  int write_status;
  struct proxy_socket *my_sock = (struct proxy_socket*) socks;

  /* Read from client_socket_fd and write to fd */
  while(1) {
    data_len = recv(my_sock->cl_sock_fd, buffer, PROXY_BUFFER_SIZE - 1, 0);
    if(data_len <= 0) { // Read failed, break
      break;
    }
    else {
      write_status = http_send_data(my_sock->fd, buffer, data_len);
    }

    if(write_status < 0) { // Write failed, break
      break;
    }
  }

  /* Inform peer thread that we will not be writing anymore, so read in 
  down stream fails and exits the while loop */
  shutdown(my_sock->fd, SHUT_WR);

  /* We wait for peer thread to finish */
  pthread_mutex_lock(my_sock->status_check);
  if(*my_sock->finished == 0) {
    *(my_sock->finished) = 1;
    pthread_cond_wait(my_sock->status_wait, my_sock->status_check);
  }
  else if(*my_sock->finished == 1) {
    *(my_sock->finished) = 2;
    pthread_cond_signal(my_sock->status_wait);
  }
  pthread_mutex_unlock(my_sock->status_check);

  /* Infrom parent thread to close the sockets */
  sem_post(my_sock->done);

  /* Free resources and exit */
  free(buffer);
  return NULL;
}

/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 * You can change these functions to anything you want.
 * 
 * ATTENTION: Be careful to optimize your code. Judge is
 *            sesnsitive to time-out errors.
 */
void serve_file(int fd, char *path) {

  /* Initialize the response */
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(path));

  /* Open the file and calcualte its length in bytes */
  FILE *file = fopen(path, "rb");
  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  char *file_size_str = (char *) malloc(MAX_CON_LEN);

  /* Reset file pos to the begining of the file */
  fseek(file, 0, SEEK_SET);

  /* Convert the file length to string and write the header */
  sprintf(file_size_str, "%ld", file_size);
  http_send_header(fd, "Content-Length", file_size_str); // Change this too
  http_end_headers(fd);
  free(file_size_str); // No longer needed

  /* Create a buffer to read the file contents */
  char *buffer = malloc(file_size);
  fread(buffer, sizeof(char), file_size, file);

  /* Write file contents to socket fd */
  http_send_data(fd, buffer, file_size);
  
  /* Close the file, free the buffer, and return */
  fclose(file);
  free(buffer);
}

void serve_directory(int fd, char *path) {
  /*
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd); */

  /* TODO: PART 1 Bullet 3,4 */

  /* First check if index.html exists in the directory */
  char index_file[] = "/index.html";
  char *tmp_index_path = malloc(strlen(path) + strlen(index_file) + 2);
  strcpy(tmp_index_path, path);
  strcat(tmp_index_path, index_file);
  if(access(tmp_index_path, F_OK) == 0) { // The directory contains index.html
    serve_file(fd, tmp_index_path);
  }
  else { // The directory has no index.html, so list enteries
    /* Start HTTP headers */
    http_start_response(fd, 200);
    http_send_header(fd, "Content-Type", http_get_mime_type(".html"));

    /* Get directory list and complete headers */
    char *dir_list_html = list_dirs(path);
    char *file_size_str = (char *) malloc(MAX_CON_LEN);
    sprintf(file_size_str, "%ld", strlen(dir_list_html));
    http_send_header(fd, "Content-Length", file_size_str);
    http_end_headers(fd);

    /* Send HTTP data */
    http_send_string(fd, dir_list_html);

    /* Free allocated memory */
    free(dir_list_html); // Was allocated in list_dirs function
    free(file_size_str);
  }

  /* Free allocated memory */
  free(tmp_index_path);
}


/*
 * Reads an HTTP request from stream (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 * 
 *   Closes the client socket (fd) when finished.
 */
void handle_files_request(int fd) {

  while(1) {
    /* Get the new fd from work queue */
    fd = wq_pop(&work_queue);

    /* Serve and close the fd */
    struct http_request *request = http_request_parse(fd);

    if (request == NULL || request->path[0] != '/') {
      http_start_response(fd, 400);
      http_send_header(fd, "Content-Type", "text/html");
      http_end_headers(fd);
      close(fd);
      return;
    }

    if (strstr(request->path, "..") != NULL) {
      http_start_response(fd, 403);
      http_send_header(fd, "Content-Type", "text/html");
      http_end_headers(fd);
      close(fd);
      return;
    }

    
    /* The request->path variable is relative to server_files_directory */
    char *path = malloc(strlen(server_files_directory) + strlen(request->path) + 2);
    strcpy(path, server_files_directory);
    strcat(path, request->path);

    /* Get information about the specified path */
    struct stat stat_buf;
    if(stat(path, &stat_buf)) { // If an error occures
      http_start_response(fd, 404);
      http_send_header(fd, "Content-Type", "text/html");
      http_end_headers(fd);
      close(fd);
      return;
    }
    else { // If the specified path exists
      /* If a file is specified */
      if(S_ISREG(stat_buf.st_mode)) {
        printf("Into file serve %d\n", fd);
        serve_file(fd, path);
        printf("Out of file serve %d\n", fd);
      }
      /* If a directory is specified */
      else if(S_ISDIR(stat_buf.st_mode)) {
        printf("Into directory serve %d\n", fd);
        serve_directory(fd, path);
        printf("Out of directory serve %d\n", fd);
      }
    }

    close(fd);
  }
  return;
}


/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target. HTTP requests from the client (fd) should be sent to the
 * proxy target, and HTTP responses from the proxy target should be sent to
 * the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 */
void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and 
  * opens a connection to it. Please do not modify.
  */
  while(1) {
    /* Get a new fd from work queue */
    fd = wq_pop(&work_queue);
    printf("Start proxy %d\n", fd);

    /* Serve and close fd */
    struct sockaddr_in target_address;
    memset(&target_address, 0, sizeof(target_address));
    target_address.sin_family = AF_INET;
    target_address.sin_port = htons(server_proxy_port);

    struct hostent *target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

    int client_socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (client_socket_fd == -1) {
      fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
      close(fd);
      exit(errno);
    }

    if (target_dns_entry == NULL) {
      fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
      close(fd);
      exit(ENXIO);
    }

    char *dns_address = target_dns_entry->h_addr_list[0];

    memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
    int connection_status = connect(client_socket_fd, (struct sockaddr*) &target_address,
        sizeof(target_address));

    if (connection_status < 0) {
      /* Dummy request parsing, just to be compliant. */
      http_request_parse(fd);

      http_start_response(fd, 502);
      http_send_header(fd, "Content-Type", "text/html");
      http_end_headers(fd);
      http_send_string(fd, "<center><h1>502 Bad Gateway</h1><hr></center>");
      close(fd);
      return;

    }

    /* Create two thread handles, upstream will send data from fd to client_socket_fd
    * and downstream will do the reverse */
    pthread_cond_t proxy_cond;
    pthread_mutex_t proxy_mutex;
    pthread_cond_init(&proxy_cond, NULL);
    pthread_mutex_init(&proxy_mutex, NULL);

    int finished = 0;

    struct proxy_socket up_sockets;
    up_sockets.cl_sock_fd = client_socket_fd; up_sockets.fd = fd;
    up_sockets.status_wait =  &proxy_cond; 
    up_sockets.finished = &finished;
    up_sockets.status_check = &proxy_mutex;

    sem_t proxy_done;
    sem_init(&proxy_done, 0, 0);

    up_sockets.done = &proxy_done;
    pthread_t upstream_thread, downstream_thread;

    /* Create read/write threads for current request */
    pthread_create(&upstream_thread, NULL, proxy_thread_handle_upstream, &up_sockets);
    pthread_create(&downstream_thread, NULL, proxy_thread_handle_downstream, &up_sockets);

    /* Wait for threads to signal finish */
    sem_wait(&proxy_done);

    /* Wait for threads to free their resources and completely finish */
    pthread_join(upstream_thread, NULL);
    pthread_join(downstream_thread, NULL);

    /* Close the sockets properly */
    close(fd);
    close(client_socket_fd);
    
    /* Destroy synchronization primitives used */
    sem_destroy(&proxy_done);
    pthread_mutex_destroy(&proxy_mutex);
    pthread_cond_destroy(&proxy_cond);
  }
  

  /* 
  * TODO: Your solution for task 3 belongs here! 
  */
}


void init_thread_pool(int num_threads, void (*request_handler)(int)) {
  /*
   * TODO: Part of your solution for Task 2 goes here!
   */

  /* Allocate the array of thread handles of the server thread pool */
  threads = malloc(num_threads * sizeof(pthread_t));

  /* Initially create threads with fake client socket number 0
    * In request handler function, they constantly pop socket numbers from wq */
  for(int i = 0; i < num_threads; i++) {
    pthread_create(&threads[i], NULL, request_handler, 0);
  }
}

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int *socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option,
        sizeof(socket_option)) == -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  if (bind(*socket_number, (struct sockaddr *) &server_address,
        sizeof(server_address)) == -1) {
    perror("Failed to bind on socket");
    exit(errno);
  }

  if (listen(*socket_number, 1024) == -1) {
    perror("Failed to listen on socket");
    exit(errno);
  }

  printf("Listening on port %d...\n", server_port);

  /* Must come before init_thread_pool to make sure cond & mutex vars are
  * initialized correctly */
  wq_init(&work_queue);

  init_thread_pool(num_threads, request_handler);

  while (1) {
    client_socket_number = accept(*socket_number,
        (struct sockaddr *) &client_address,
        (socklen_t *) &client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    // printf("Accepted connection from %s on port %d\n",
    //     inet_ntoa(client_address.sin_addr),
    //     client_address.sin_port);
/*
    // TODO: Change me?
    request_handler(client_socket_number);
    close(client_socket_number); */

    wq_push(&work_queue, client_socket_number);

    printf("Accepted connection from %s on port %d\n",
        inet_ntoa(client_address.sin_addr),
        client_address.sin_port);
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0) perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char *USAGE =
  "Usage: ./httpserver --files www_directory/ --port 8000 [--num-threads 5]\n"
  "       ./httpserver --proxy inst.eecs.berkeley.edu:80 --port 8000 [--num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;
  num_threads = 1; // At least one thread to handle requests

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      free(server_files_directory);
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char *proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char *colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char *server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char *num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
