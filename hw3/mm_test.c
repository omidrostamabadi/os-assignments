/* A simple test harness for memory alloction. */

#include "mm_alloc.h"
#include <stdio.h>
#include <unistd.h>

#define ASSERT(x, y) if(x != y) printf("Asserting failed in %s %d\n", __FILE__, __LINE__)

extern s_block_ptr start_list;
void print_list() {
  if(start_list == NULL) {
    printf("Nothing to print\n");
    return;
  }
  s_block_ptr tmp_blk = start_list;
  printf("*********** LIST PRINT ***********\n");
  while(tmp_blk != NULL) {
    int if_free = tmp_blk->is_free;
    printf("free: %s size: %lu\n", if_free ? "FREE" : "OCCU", tmp_blk->size);
    tmp_blk = tmp_blk->next;
  }
  printf("*********** LIST PRINT END ***********\n\n");
}

void check_zero(char *ptr, size_t size) {
  if(ptr == NULL) {
    printf("NULL to check_zero\n");
    return;
  }
  for(size_t i = 0; i < size; i++) {
    if(ptr[i] != 0)
      printf(" -------- Detected nonzero byte! --------- i = %lu\n", i);
  }
}

int main(int argc, char **argv)
{
    int *data, *a, *b, *c, *d;
    int *mnull = mm_malloc(0);
    ASSERT(mnull, NULL);
    mnull = mm_realloc(NULL, 0);
    ASSERT(mnull, NULL);


    a = mm_malloc(40);
    print_list();
    mm_free(a);
    print_list();
    b = mm_malloc(30);
    print_list();
    mm_free(b);
    print_list();
    c = mm_malloc(20);
    print_list();
    mm_free(c);
    print_list();
   data = (int*) mm_realloc(NULL, 800000000000000000);
   ASSERT(data, NULL);
   check_zero(data, 800000000000000000);
   print_list();
   a = mm_malloc(12);
   check_zero(a, 12);
   print_list();
   mm_free(data);
   print_list();
   mnull = (int*) mm_realloc(NULL, 800000000000000000);
   ASSERT(mnull, NULL);
  // print_list();

   b = mm_malloc(20);
   check_zero(b, 20);
   print_list();
   c = mm_realloc(NULL, 40);
   check_zero(c, 40);
   print_list();
   mnull = (int*) mm_realloc(NULL, 800000000000000000);
   ASSERT(mnull, NULL);
  // print_list();

   data = (int*) mm_malloc(16000);
   check_zero(data, 16000);
   print_list();
   mm_free(data);
   print_list();
   mnull = (int*) mm_realloc(NULL, 800000000000000000);
   ASSERT(mnull, NULL);
  // print_list();

   mm_free(a);
   print_list();
   mm_free(c);
   print_list();
   mnull = (int*) mm_realloc(NULL, 800000000000000000);
   ASSERT(mnull, NULL);
  // print_list();

   d = mm_malloc(100);
   check_zero(d, 100);
   print_list();
   mnull = (int*) mm_realloc(NULL, 800000000000000000);
   ASSERT(mnull, NULL);
  // print_list();
   mm_free(b); 
   print_list();
   mm_free(d);
   print_list();
  //  data = (int*) mm_malloc(32000);
  //  mm_free(data);
  //  data = (int*) mm_malloc(64000);
  //  mm_free(data);
  //  mm_free(data);
  //  data = (int*) mm_malloc(128000);
  //  mm_realloc(data, 12);
  //  data[2] = 98;
  //  printf("data %d %d %d\n", data[0], data[1], data[2]);
  //  mm_free(data);
   printf("Hooray! malloc passed!\n");
  //  data[0] = 1;
  //  mm_free(data);
  //  printf("malloc sanity test successful!\n");

  /* Test area */
  //printf("%d\n", getpagesize());
  // char *test;
  // test = sbrk(0);
  // printf("Current position of heap: %lx\n", test);
  // test = sbrk(0);
  // printf("Current position of heap: %lx\n", test);
  // test = sbrk(0);
  // printf("Current position of heap: %lx\n", test);
  // test = (char*) sbrk(16);
  // printf("Current position of heap: %lx\n", test);
  // test[0] = 'a'; test[1] = 'b'; test[2] = '\0';
  // printf("%s saved\n", test);
  // test = sbrk(0);
  // printf("Current position of heap: %lx\n", test);

  // test = sbrk(32);
  // printf("Current position of heap: %lx\n", test);
  // test = sbrk(0);
  // printf("Current position of heap: %lx\n", test);
  // test = sbrk(0);
  // printf("Current position of heap: %lx\n", test);
    return 0;
}
