#include <stdlib.h>
#include "wq.h"
#include "utlist.h"

/* Initializes a work queue WQ. */
void wq_init(wq_t *wq) {

  /* TODO: Make me thread-safe! */
  pthread_mutex_init(&wq_mutex, NULL); // Init mutex with default attrs
  pthread_cond_init(&wq_empty, NULL); // Init cond with default attrs
  pthread_mutex_lock(&wq_mutex);
  wq->size = 0;
  wq->head = NULL;
  pthread_mutex_unlock(&wq_mutex);
}

/* Remove an item from the WQ. This function should block until there
 * is at least one item on the queue. */
int wq_pop(wq_t *wq) {

  /* TODO: Make me blocking and thread-safe! */
  // printf("Trying to pop...\n");
  /* Acquire the lock to proceed safely */
  pthread_mutex_lock(&wq_mutex);

  /* Wait untill there's at least one element in the work queue */
  while(wq->size == 0) {
    // printf("Going to wait...\n");
    pthread_cond_wait(&wq_empty, &wq_mutex);
    // printf("Waking up from wait...\n");
  }

  /* Get the item in the work queue */
  wq_item_t *wq_item = wq->head;
  int client_socket_fd = wq->head->client_socket_fd;
  wq->size--;
  DL_DELETE(wq->head, wq->head);

  /* Release lock and resources and return */
  pthread_mutex_unlock(&wq_mutex);
  free(wq_item);
  // printf("Poped item, returning...\n");
  return client_socket_fd;
}

/* Add ITEM to WQ. */
void wq_push(wq_t *wq, int client_socket_fd) {

  /* TODO: Make me thread-safe! */
  // printf("Trying to push...\n");
  /* Acquire the lock to proceed safely */
  pthread_mutex_lock(&wq_mutex);

  /* insert the item into the work queue */
  wq_item_t *wq_item = calloc(1, sizeof(wq_item_t));
  wq_item->client_socket_fd = client_socket_fd;
  DL_APPEND(wq->head, wq_item);
  wq->size++;

  /* If size is 1, need to signal, maybe some threads are wainting! */
  if(wq->size == 1)
    pthread_cond_signal(&wq_empty);

  /* Release the lock */
  pthread_mutex_unlock(&wq_mutex);
  // printf("Pushed, returning...\n");
}
