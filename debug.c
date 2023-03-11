
#include <stdio.h>
#include "debug.h"

int debug_level = 0;

void debug(char *debug_text_in, int debug_text_level){

  char temp_char[DEBUG_TEXT_SIZE+21];


  // substitute carriage returns and newlines
  int x = 0;
  int y = 0;
  while ((x < DEBUG_TEXT_SIZE) && (debug_text_in[x] != 0) && (y < (DEBUG_TEXT_SIZE+20))){
    if (debug_text_in[x] == '\r'){
      temp_char[y] = '\\';
      y++;     
      temp_char[y] = 'r';
      y++;
    } else if (debug_text_in[x] == '\n'){
      temp_char[y] = '\\';
      y++;   
      temp_char[y] = 'n';
      y++;
    } else {
      temp_char[y] = debug_text_in[x];
      y++;
    }
    x++;
  }
  temp_char[y] = 0;


  if (debug_text_level > 254){ // this debug text is to go out STDERR
    fprintf(stderr, temp_char);
    fprintf(stderr, "\r\n");
    fflush(stderr);
  } else {
    if (debug_text_level <= debug_level){
    	printf(temp_char);
    	printf("\r\n");
      fflush(stdout);
    }
  }

}
