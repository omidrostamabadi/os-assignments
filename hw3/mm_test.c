/* A simple test harness for memory alloction. */

#include "mm_alloc.h"
#include <stdio.h>
#include <unistd.h>
int main(int argc, char **argv)
{
    int *data;

   data = (int*) mm_malloc(8);
   data[0] = 68;
   data[1] = 78;
   printf("data %d %d\n", data[0], data[1]);
   mm_realloc(data, 12);
   data[2] = 98;
   printf("data %d %d %d\n", data[0], data[1], data[2]);
   mm_free(data);
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
