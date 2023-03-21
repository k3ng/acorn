#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include "k3ng.h"



void print_hex(int hex_in){
  printf("print_hex:%#010x\r\n", hex_in); 
}

long linux_millis(){
    struct timespec _t;
    clock_gettime(CLOCK_REALTIME, &_t);
    return _t.tv_sec*1000 + lround(_t.tv_nsec/1e6);
}

int safe_strcat(char *goes_outta, char *goes_inta, int bytes){

  if ((strlen(goes_outta) + strlen(goes_inta)) < (bytes-1)){
  	strcat(goes_outta,goes_inta);
  	return 0;
  } else {
  	return -1;
  }

}



int ispointerOK(void *pointer_to_test){

  int filehandle = open(pointer_to_test,0,0);
  int errornumber = errno;

  if ((filehandle == -1) && (errornumber == EFAULT)){
    //printf("bad pointer: %p\n", pointer_to_test);
    return 0;
  } else if (filehandle != -1){
    close(filehandle);
  }

  //printf("good pointer: %p\n",pointer_to_test);
  return 1;

}