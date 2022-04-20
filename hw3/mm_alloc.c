/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines. Remove this comment and provide
 * a summary of your allocator's design here.
 */

#include "mm_alloc.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>

/* Your final implementation should comment out this macro. */
//#define MM_USE_STUBS

#define PAGE_SIZE 4096

/* Start of the list managed by memory allocator */
 s_block_ptr start_list = NULL;

/* Helper functions are defined here */

/* 
 * Add a block of size s at the end of the heap.
 * Last should be last block in list, meaning that 
 * next pointer should not point to anything meaningful.
 * Returns NULL if things go wrong.
*/
s_block_ptr extend_heap (s_block_ptr last , size_t s) {
  /* Request allocation of the new block */
  s_block_t *new_block = sbrk(s + BLOCK_SIZE);

  if(new_block == (void*)-1) // Cannot extend heap
    return NULL;
  
  /* Add the block to heap linked list before returning */
  new_block->free = TRUE;
  new_block->prev = NULL;
  new_block->size = s;
  last->next = new_block;

  return new_block;
}

void split_block (s_block_ptr b, size_t s) {
  if(b->size <= s + BLOCK_SIZE) // Cannot split block
    return;
  
  s_block_ptr second_block = b->data[s]; // Keep s bytes for current block

  second_block->free = TRUE; // Mark new block as free

  /* Update size of blocks properly */
  second_block->size = b->size - s;
  b->size = s;

  /* Put new block as the next block of current block being split */
  second_block->next = b->next;
  b->next = second_block;
  second_block->prev = b;
}

s_block_ptr fusion(s_block_ptr b) {
  /* Try fusion with next neighbour */
  if(b->next != NULL) {
    if(b->next->free == TRUE) {
      b->next = b->next->next;
      /* Accumulate data segments, and also mix two meta data as one,
      so BLOCK_SIZE bytes will be added to the final size */
      b->size = b->size + b->next->size + BLOCK_SIZE;
    }
  }

  /* Try fusion with previous neighbour */
  if(b->prev != NULL) {
    if(b->prev->free == TRUE) {
      b->prev = b->prev->prev;
      b->size = b->size + b->prev->size + BLOCK_SIZE;
    }
  }
  
  return b;
}

s_block_ptr get_block (void *p) {
  char *tmp_data = (char*) p;
  tmp_data -= BLOCK_SIZE;
  return (s_block_ptr)tmp_data;
}

s_block_ptr get_free_block(size_t size) {
  if(start_list == NULL) { // First call to malloc
    // s_block_ptr test_ptr = sbrk(0); // This call is needed, don't know why!
    // printf("Start of heap: %lx\n", (u_int64_t)test_ptr);

    start_list = sbrk(PAGE_SIZE);
    printf("Start_list: %lx\n", (u_int64_t)start_list);

    // test_ptr = sbrk(0); // This call is needed, don't know why!
    // printf("Start of heap: %lx\n", (u_int64_t)test_ptr);

    // test_ptr = &(start_list->free);
    // printf("Address of free: %lx\n", (u_int64_t)test_ptr);

    if(start_list == (void*)-1) // sbrk failed
      return NULL;
    
    /* Set meta data of the struct */
    start_list->size = size;
    start_list->free = FALSE;
    start_list->next = NULL;
    start_list->prev = NULL;
    

    return start_list;
  }

  /* Traverse the list to get a free block of at least that size */
  s_block_ptr first_fit = start_list;
  int block_found = FALSE;
  do {
    if(first_fit->size >= size && first_fit->free == TRUE) {
      block_found = TRUE;
      break;
    }
    if(first_fit->next != NULL)
      first_fit = first_fit->next;
    else
      break;
  } while(1);

  if(block_found == TRUE) {
    split_block(first_fit, size);
    first_fit->free = FALSE;
    return first_fit;
  }
  else { // There's no block with enough size, extend heap
    s_block_ptr new_block = extend_heap(first_fit, size);
    if(new_block == NULL)
      return NULL;
    
    first_fit->next = new_block;
    new_block->prev = first_fit;
    new_block->size = size;
    new_block->free = FALSE;
    new_block->next = NULL;
    return new_block;
  }

}

void* mm_malloc(size_t size)
{
#ifdef MM_USE_STUBS
    return calloc(1, size);
#else
  // struct rlimit rlim;
  // if(getrlimit(RLIMIT_DATA, &rlim)) printf("ERRR\n");

  // printf("RLIM: curr: %ld  max: %ld\n", rlim.rlim_cur, rlim.rlim_max);

  char *test_ptr = sbrk(0);
  printf("Start of heap: %lx\n", (u_int64_t)test_ptr);

  if(size == 0)
    return NULL;

  s_block_ptr free_block = get_free_block(size);

  /* Neither a free block of that size can be found, nor can extend 
  the heap to accomodate the need. */
  if(free_block == NULL) 
    return NULL;
  
  memset(free_block->data, 0, size); // Zero fill new block
  printf("Address of free block: %lx (data = %lx)\n", 
  (u_int64_t)free_block, (u_int64_t)&(free_block->data));
  return (void*)(free_block->data);
#endif
}

/* 
* The behaviour is defined due to linux man page for this function in C. 
* If this fails, the original memory is untouched, neither free, nor moved.
*/
void* mm_realloc(void* ptr, size_t size)
{
#ifdef MM_USE_STUBS
    return realloc(ptr, size);
#else
if(size == 0) {
  mm_free(ptr);
  return NULL;
}

if(ptr == NULL) {
  return mm_malloc(size);
}

s_block_ptr primary_block;
s_block_ptr secondary_block;
void *sec_ptr;

/* Get the block from user ptr */
primary_block = get_block(ptr);
size_t old_size = primary_block->size;

sec_ptr = mm_malloc(size);
if(sec_ptr == NULL)
  return NULL;

if(old_size <= size) {
  memcpy(sec_ptr, ptr, old_size); // Move data to new block
  memset((char*)sec_ptr + old_size, 0, size - old_size); // Zero tail of new block
}
else {
  memcpy(sec_ptr, ptr, size);
}
mm_free(ptr);

return sec_ptr;


#endif
}

void mm_free(void* ptr)
{
#ifdef MM_USE_STUBS
    free(ptr);
#else
  if(ptr == NULL)
    return;
  
  s_block_ptr block_to_free = get_block(ptr);
  printf("Address of block to free: %lx (data = %lx)\n", 
  (u_int64_t)block_to_free, (u_int64_t)&(block_to_free->data));
  block_to_free->free = TRUE;
  fusion(block_to_free);
#endif
}
