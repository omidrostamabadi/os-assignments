
#include <assert.h>
#include <getopt.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>


int num_words(FILE* infile);


int main() {
//  const char *file_name = "ftest.txt";
//  FILE *f = fopen(file_name, "r");
  
  // char c = (char)fgetc(f);
  // while(c != EOF) {
  //   printf("This char = %c\n", c);
  //   c = (char)fgetc(f);
  // }

  const char *s1 = "omid";
  const char *s2 = "bmid";
  int res = strcmp(s1, s2);
  printf("result: %d\n", res);

  return 0;
}

int num_words(FILE* infile) {
  int num_words = 0;
  char c = 'A'; // Current char
  char prev_c = '2'; // Hold previous char
  int char_num = 0; // Number of chars in each word
  
  while(c != EOF) {
    c = fgetc(infile);
    if(isalpha(c)) {
      char_num += 1;
    }
    if(!isalpha(c) && isalpha(prev_c)) { // End of word
      if(char_num > 1) { // Has more than one char
        num_words += 1;
      }
      char_num = 0; // Reset to count next word
    }
    prev_c = c;
  }

  return num_words;
}