/*


  Standalone compile:
 
  gcc -g -o circular_buffer circular_buffer.c 


*/

#include <stdlib.h>
#include <string.h>
#include "acorn.h"
#include "circular_buffer.h"

// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------


void circular_buffer_empty(struct circular_buffer_struct *circular_buffer){

  circular_buffer->head = 0;
  circular_buffer->tail = 0;
	circular_buffer->underflow = 0;
	circular_buffer->overflow = 0;

}

// ---------------------------------------------------------------------------------------

void circular_buffer_init(struct circular_buffer_struct *circular_buffer, int length){

	circular_buffer->buffer_size = length;
	circular_buffer->data = malloc((length+1) * sizeof(char));
	memset(circular_buffer->data, 0, circular_buffer->buffer_size+1);
	circular_buffer_empty(circular_buffer);

}

// ---------------------------------------------------------------------------------------

int circular_buffer_length(struct circular_buffer_struct *circular_buffer){

  if (circular_buffer->head >= circular_buffer->tail){
    return circular_buffer->head - circular_buffer->tail;
  } else {
    return ((circular_buffer->head + circular_buffer->buffer_size) - circular_buffer->tail);
  }

}

// ---------------------------------------------------------------------------------------

int circular_buffer_write(struct circular_buffer_struct *circular_buffer, char byte_to_write){

  if (circular_buffer->head + 1 == circular_buffer->tail || circular_buffer->tail == 0 && circular_buffer->head == circular_buffer->buffer_size-1){
    circular_buffer->overflow++;
    return RETURN_ERROR;
  }

  circular_buffer->data[circular_buffer->head++] = byte_to_write;

  if (circular_buffer->head > circular_buffer->buffer_size){
    circular_buffer->head = 0;
  }
	return RETURN_NO_ERROR;

}

// ---------------------------------------------------------------------------------------

char circular_buffer_read(struct circular_buffer_struct *circular_buffer){
 
  char data;

  if (circular_buffer->tail == circular_buffer->head){
    circular_buffer->underflow++;
    return 0;
  }
    
  data = circular_buffer->data[circular_buffer->tail++];
  if (circular_buffer->tail > circular_buffer->buffer_size)
    circular_buffer->tail = 0;

  return data;

}

// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------


#if !defined(COMPILING_EVERYTHING)

  int main(int argc, char *argv[]){

  }

#endif

