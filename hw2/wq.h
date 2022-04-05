#ifndef __WQ__
#define __WQ__

#include <pthread.h>

/* Synchronization primitives used in wq */
pthread_mutex_t wq_mutex; // Lock of work queue
pthread_cond_t wq_empty; // Used to wait when work queue is empty

/* WQ defines a work queue which will be used to store accepted client sockets
 * waiting to be served. */

typedef struct wq_item {
  int client_socket_fd; // Client socket to be served.
  struct wq_item *next;
  struct wq_item *prev;
} wq_item_t;

typedef struct wq {
  int size;
  wq_item_t *head;
  /* TODO: More stuff here, maybe? */
} wq_t;

void wq_init(wq_t *wq);
void wq_push(wq_t *wq, int client_socket_fd);
int wq_pop(wq_t *wq);

#endif
