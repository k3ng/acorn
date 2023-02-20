
#include <stdio.h>

int debug_level = 0;

void debug(char *debug_text_in, int debug_text_level){

  if (debug_text_level > 254){ // this debug text is to go out STDERR
    fprintf(stderr, debug_text_in);
    fprintf(stderr, "\r\n");
    fflush(stderr);
  } else {
    if (debug_text_level <= debug_level){
    	printf(debug_text_in);
    	printf("\r\n");
      fflush(stdout);
    }
  }

}
