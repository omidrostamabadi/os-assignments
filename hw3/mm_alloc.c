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
#include <inttypes.h>

/* Your final implementation should comment out this macro. */
//#define MM_USE_STUBS

/* Start of the list managed by memory allocator */
 s_block_ptr start_list = NULL;

/* Helper functions are defined here */

char *get_data_ptr(char *block_ptr) {
  return (block_ptr + BLOCK_SIZE);
}

/* 
 * Add a block of size s at the end of the heap.
 * Last should be last block in list, meaning that 
 * next pointer should not point to anything meaningful.
 * Returns NULL if things go wrong.
*/
s_block_ptr extend_heap (s_block_ptr last , size_t s) {
  /* Request allocation of the new block */
  s_block_ptr new_block = sbrk(s + BLOCK_SIZE);

  if(new_block == (void*)-1) // Cannot extend heap
    return NULL;
  
  /* Add the block to heap linked list before returning */
  new_block->is_free = TRUE;
  new_block->next = NULL;
  new_block->prev = last;
  new_block->size = s;
  last->next = new_block;

  return new_block;
}

void split_block (s_block_ptr b, size_t s) {
  if(b->size <= s + BLOCK_SIZE) { // Cannot split block
    memset(get_data_ptr(b), 0, b->size);
    return;
  }
  
  s_block_ptr second_block = (get_data_ptr(b) + s); // Keep s bytes for current block

  second_block->is_free = TRUE; // Mark new block as is_free

  /* Update size of blocks properly */
  second_block->size = b->size - s - BLOCK_SIZE;
  b->size = s;

  /* Put new block as the next block of current block being split */
  s_block_ptr past_b = b->next;
  second_block->next = past_b;
  second_block->prev = b;
  b->next = second_block;
  if(past_b != NULL) {
    past_b->prev = second_block;
  }
}

s_block_ptr fusion(s_block_ptr b) {
  /* Try fusion with next neighbour */
  while(b->next != NULL) {
    if(b->next->is_free == TRUE) {
      size_t next_size = b->next->size;
      s_block_ptr past_next = b->next->next;
      b->next = past_next;
      if(past_next != NULL) {
        past_next->prev = b;
      }
      /* Accumulate data segments, and also mix two meta data as one,
      so BLOCK_SIZE bytes will be added to the final size */
      b->size = b->size + next_size + BLOCK_SIZE;
    }
    else {
      break;
    }
  }

  /* Try fusion with previous neighbour */
  while(b->prev != NULL) {
    if(b->prev->is_free == TRUE) {
      size_t prev_size = b->prev->size;
      s_block_ptr pre_prev = b->prev->prev;
      b->prev = pre_prev;
      if(pre_prev != NULL) {
        pre_prev->next = b;
      }
      b->size = b->size + prev_size + BLOCK_SIZE;
    }
    else {
      break;
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
  if(start_list == NULL) { // First call to mm_malloc

    start_list = sbrk(size + BLOCK_SIZE);

    if(start_list == (void*)-1) // sbrk failed
      return NULL;
    
    /* Set meta data of the struct */
    start_list->size = size;
    start_list->is_free = FALSE;
    start_list->next = NULL;
    start_list->prev = NULL;
    

    return start_list;
  }

  /* Traverse the list to get a is_free block of at least that size */
  s_block_ptr first_fit = start_list;
  int block_found = FALSE;
  do {
    if(first_fit->size >= size && first_fit->is_free == TRUE) {
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
    first_fit->is_free = FALSE;
    return first_fit;
  }
  else { // There's no block with enough size, extend heap
    s_block_ptr new_block = extend_heap(first_fit, size);
    if(new_block == NULL)
      return NULL;

    new_block->is_free = FALSE;

    return new_block;
  }

}

void* mm_malloc(size_t size)
{
  char *test_ptr = sbrk(0);

  if(size == 0)
    return NULL;

  s_block_ptr free_block = get_free_block(size);

  /* Neither a is_free block of that size can be found, nor can extend 
  the heap to accomodate the need. */
  if(free_block == NULL) 
    return NULL;
  
  memset(get_data_ptr(free_block), 0, size); // Zero fill new block

  return (void*)(get_data_ptr(free_block));
}

/* 
* The behaviour is defined due to linux man page for this function in C. 
* If this fails, the original memory is untouched, neither is_free, nor moved.
*/
void* mm_realloc(void* ptr, size_t size)
{
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
}

void mm_free(void* ptr)
{
  if(ptr == NULL)
    return;
  
  s_block_ptr block_to_free = get_block(ptr);
  block_to_free->is_free = TRUE;
  fusion(block_to_free);
}
