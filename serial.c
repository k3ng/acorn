/*


  Serial Port Management

  Anthony Good, K3NG


  How this all works:

    setup_serial_port() is called to initialize a serial port.  
    This sets up the parameters of the port and creates a serial_buffer_struct for incoming data.  It also launches a
    serial_incoming_thread which waits for serial data coming in and writes it to the serial_buffer_struct.  one 
    serial_incoming_thread is launched for each serial port that is setup.

    add_to_serial_incoming_buffer() is used by serial_incoming_thread() to add serial port bytes received into serial_buffer_struct

    get_from_serial_incoming_buffer() is used to read received serial port bytes and clear the buffer for that port    

    send_out_serial_port() is used to send data out a serial port; there is no outgoing buffer, we just send it right out

    No attempt is made here to match up responses to messages that were sent out.  That's up to the subroutines that call 
    send_out_serial_port() and get_from_serial_incoming_buffer().


  Standalone compile with:

    gcc -g -o serial serial.c -pthread

  The standalone compilation tests the connection to the AVR on /dev/ttyS0 by sending the p (poll) command in a loop.


*/



#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <pthread.h>


// ---------------------------------------------------------------------------------------



struct serial_buffer_struct{
  int fd;                                           // serial port file descriptor
  char portname[16];                                // port name i.e. /dev/ttyS0
  char incoming_buffer[100];
  void *incoming_buffer_spin_lock;   // 0 = no lock
  struct serial_buffer_struct* next_serial_buffer;  // pointer to the next serial buffer
};

struct serial_buffer_struct *serial_buffer_first = NULL; //may have to declare extern in serial.h?


// ---------------------------------------------------------------------------------------


#if !defined(COMPILING_EVERYTHING)

  
  int shutdown_flag = 0;


  void debug(char *debug_text_in, int debug_text_level){

    printf(debug_text_in);
    printf("\r\n");
    fflush(stdout);

  }

#else

  #include "acorn.h"

#endif //COMPILING_EVERYTHING

// ---------------------------------------------------------------------------------------
int send_out_serial_port(char *whichone,char *stuff_to_send){

  /* whichone can be either the portname (i.e. /dev/ttyS0) or the file descriptor ("fd:3") */

  
  struct serial_buffer_struct *serial_buffer_temp = serial_buffer_first;

  char temp_char_fd[16];

  while(1){
    sprintf(temp_char_fd,"fd:%d",serial_buffer_temp->fd);  // make the format of the file descriptor

    // try to match either the portname (i.e. /dev/ttyS0) or the file descriptor ("fd:3")
    if ((!strcmp(whichone,serial_buffer_temp->portname)) || (!strcmp(whichone,temp_char_fd))){ 
      // found a match, add stuff_to_add to the buffer
      write (serial_buffer_temp->fd, stuff_to_send, strlen(stuff_to_send));
      return 1;
    } else {
      if (serial_buffer_temp->next_serial_buffer != NULL){
         serial_buffer_temp = serial_buffer_temp->next_serial_buffer;  // get the next serial port buffer
      } else {
        // no match
        return -1;
      }
    }
  }

}


// ---------------------------------------------------------------------------------------
int add_to_serial_incoming_buffer(char *whichone,char *stuff_to_add){

  /* whichone can be either the portname (i.e. /dev/ttyS0) or the file descriptor ("fd:3") */

  
  struct serial_buffer_struct *serial_buffer_temp = serial_buffer_first;

  char temp_char_fd[16];

  while(1){
    sprintf(temp_char_fd,"fd:%d",serial_buffer_temp->fd);  // make the format of the file descriptor

    // try to match either the portname (i.e. /dev/ttyS0) or the file descriptor ("fd:3")
    if ((!strcmp(whichone,serial_buffer_temp->portname)) || (!strcmp(whichone,temp_char_fd))){ 
      // found a match, add stuff_to_add to the buffer
      while(serial_buffer_temp->incoming_buffer_spin_lock != add_to_serial_incoming_buffer){ // spin lock the struct for writing
        if (serial_buffer_temp->incoming_buffer_spin_lock == 0){
          serial_buffer_temp->incoming_buffer_spin_lock = add_to_serial_incoming_buffer;
        }
        usleep(5);
      }      
      strcat(serial_buffer_temp->incoming_buffer,stuff_to_add);
      serial_buffer_temp->incoming_buffer_spin_lock = 0;
      return 1;
    } else {
      if (serial_buffer_temp->next_serial_buffer != NULL){
         serial_buffer_temp = serial_buffer_temp->next_serial_buffer;  // get the next serial port buffer
      } else {
        // no match
        return -1;
      }
    }
  }

}

// ---------------------------------------------------------------------------------------
int get_from_serial_incoming_buffer_everything(char *whichone,char *stuff_to_get){

  /* whichone can be either the portname (i.e. /dev/ttyS0) or the file descriptor ("fd:3") */

  
  struct serial_buffer_struct *serial_buffer_temp = serial_buffer_first;

  char temp_char_fd[16];

  strcpy(stuff_to_get,"");

  while(1){
    sprintf(temp_char_fd,"fd:%d",serial_buffer_temp->fd);

    // try to match either the portname (i.e. /dev/ttyS0) or the file descriptor ("fd:3")
    if ((!strcmp(whichone,serial_buffer_temp->portname)) || (!strcmp(whichone,temp_char_fd))){ 
      // found a match
      if (strlen(serial_buffer_temp->incoming_buffer) > 0){
        while(serial_buffer_temp->incoming_buffer_spin_lock != get_from_serial_incoming_buffer_everything){ // spin lock the struct for reading
          if (serial_buffer_temp->incoming_buffer_spin_lock == 0){
            serial_buffer_temp->incoming_buffer_spin_lock = get_from_serial_incoming_buffer_everything;
          }
          usleep(5);
        }
        strcpy(stuff_to_get,serial_buffer_temp->incoming_buffer);
        strcpy(serial_buffer_temp->incoming_buffer,"");       // clear the buffer
        serial_buffer_temp->incoming_buffer_spin_lock = 0;  // unlock the spin lock
        return 1;
      } else {
        return 0;
      }
    } else {
      if (serial_buffer_temp->next_serial_buffer != NULL){
         serial_buffer_temp = serial_buffer_temp->next_serial_buffer;  // get the next serial port buffer
      } else {
        // no match
        return -1;
      }
    }
  }

}

// ---------------------------------------------------------------------------------------
int get_from_serial_incoming_buffer_one_line(char *whichone,char *stuff_to_get){

  /* whichone can be either the portname (i.e. /dev/ttyS0) or the file descriptor ("fd:3") */

  
  struct serial_buffer_struct *serial_buffer_temp = serial_buffer_first;

  char temp_char_fd[16];

  strcpy(stuff_to_get,"");

  while(1){
    sprintf(temp_char_fd,"fd:%d",serial_buffer_temp->fd);

    // try to match either the portname (i.e. /dev/ttyS0) or the file descriptor ("fd:3")
    if ((!strcmp(whichone,serial_buffer_temp->portname)) || (!strcmp(whichone,temp_char_fd))){ 
      // found a match
      // is there a return in the buffer?
      char *returnchar = strchr(serial_buffer_temp->incoming_buffer, '\r');
      
      if (!returnchar){
        // no return in the buffer
        return 0;
      }

      if (strlen(serial_buffer_temp->incoming_buffer) > 0){
        while(serial_buffer_temp->incoming_buffer_spin_lock != get_from_serial_incoming_buffer_one_line){ // spin lock the struct for reading
          if (serial_buffer_temp->incoming_buffer_spin_lock == 0){
            serial_buffer_temp->incoming_buffer_spin_lock = get_from_serial_incoming_buffer_one_line;
          }
          usleep(5);
        }
        // we got the spin lock
        int number_of_bytes = returnchar - serial_buffer_temp->incoming_buffer;
        //char one_line[64];
        strncpy(stuff_to_get, serial_buffer_temp->incoming_buffer, number_of_bytes);
        stuff_to_get[number_of_bytes] = 0;
        
        strcpy(serial_buffer_temp->incoming_buffer, serial_buffer_temp->incoming_buffer+number_of_bytes+1);
        // strip \n if it's there
        char *newlinechar = strchr(serial_buffer_temp->incoming_buffer, '\n');  
        if (newlinechar){
          strcpy(serial_buffer_temp->incoming_buffer, serial_buffer_temp->incoming_buffer+1);
        }
        serial_buffer_temp->incoming_buffer_spin_lock = 0;  // unlock the spin lock
        return 1;
      } else {
        return 0;
      }
    } else {
      if (serial_buffer_temp->next_serial_buffer != NULL){
         serial_buffer_temp = serial_buffer_temp->next_serial_buffer;  // get the next serial port buffer
      } else {
        // no match
        return -1;
      }
    }
  }

}


// ---------------------------------------------------------------------------------------
void *serial_incoming_thread(void *passed_fd){

  char debug_text[100];
  char buffer[3];
  int fd = *(int*)passed_fd;
  char temp_fd[16];

  sprintf(temp_fd,"fd:%d",fd);

  sprintf(debug_text,"serial_incoming_thread: fd:%d launched",fd);
  debug(debug_text,2);


  while(!shutdown_flag){
    int n = read (fd, buffer, 1);
    if (n){
      add_to_serial_incoming_buffer(temp_fd, buffer);
    }
    usleep (1000);
  }

  sprintf(debug_text,"serial_incoming_thread: fd:%d exiting",fd);
  debug(debug_text,2);

}

// ---------------------------------------------------------------------------------------


int setup_serial_port(char *portname, int speed, int parity, int should_block){


  /*  

      This sets up a serial port and launches a thread, serial_incoming_thread, to
      service incoming bytes  

  */

  int *new_fd;
  char debug_text[100]; 

  int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 1){
    sprintf(debug_text,"setup_serial_port: error %d opening %s: %s", errno, portname, strerror (errno));
    debug(debug_text,255);
    return -1;
  }

  struct termios tty;
  if (tcgetattr (fd, &tty) != 0){
    sprintf(debug_text,"setup_serial_port: error %d from tcgetattr", errno);
    debug(debug_text,255);    
    return -1;
  }

  cfsetospeed (&tty, speed);
  cfsetispeed (&tty, speed);

  tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
  // disable IGNBRK for mismatched speed tests; otherwise receive break
  // as \000 chars
  tty.c_iflag &= ~IGNBRK;         // disable break processing
  tty.c_lflag = 0;                // no signaling chars, no echo,
                                  // no canonical processing
  tty.c_oflag = 0;                // no remapping, no delays
  tty.c_cc[VMIN]  = 0;            // read doesn't block
  tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

  tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl

  tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
                                  // enable reading
  tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
  tty.c_cflag |= parity;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CRTSCTS;

  if (tcsetattr (fd, TCSANOW, &tty) != 0){
    sprintf(debug_text,"setup_serial_port: error %d from tcsetattr", errno);
    debug(debug_text,255);     
    return -1;
  }


  tty.c_cc[VMIN]  = should_block ? 1 : 0;
  tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

  if (tcsetattr (fd, TCSANOW, &tty) != 0){
    sprintf(debug_text,"error %d setting term attributes", errno);
    debug(debug_text,255);     
  }


  pthread_t serial_incoming_pthread;
  new_fd = malloc(1);
  *new_fd = fd;

  if (pthread_create(&serial_incoming_pthread, NULL, serial_incoming_thread, (void*) new_fd) < 0){
    sprintf(debug_text,"setup_serial_port: could not create serial_incoming_thread");
    debug(debug_text,255);
  }

  // set up a struct for the serial port
  struct serial_buffer_struct *serial_buffer = malloc(sizeof(struct serial_buffer_struct));
  serial_buffer->fd = fd;
  serial_buffer->incoming_buffer_spin_lock = 0;
  strcpy(serial_buffer->portname,portname);
  serial_buffer->next_serial_buffer = serial_buffer_first;
  serial_buffer_first = serial_buffer;

  sprintf(debug_text,"setup_serial_port: fd:%d exiting",fd);
  debug(debug_text,2);

  return fd;
}


// ---------------------------------------------------------------------------------------


#if !defined(COMPILING_EVERYTHING)


  void main(){


    char *portname = "/dev/ttyS0";
    int fd = setup_serial_port(portname, B115200, 0, 1);  // set speed to 115200 baud, N81

    printf("struct portname: %s\r\n",serial_buffer_first->portname);

    char *portname2 = "/dev/ttyAMA0";
    int fd2 = setup_serial_port(portname2, B115200, 0, 1);  // set speed to 115200 buad, N81

    printf("struct portname: %s\r\n",serial_buffer_first->portname);


    add_to_serial_incoming_buffer("/dev/ttyS0","here is some text");
    add_to_serial_incoming_buffer("/dev/ttyAMA0","some text for AMA0 port");
    add_to_serial_incoming_buffer("fd:3"," ...and some more text");

    char tempchar[100];
    if (get_from_serial_incoming_buffer_everything("/dev/ttyS0",tempchar) > 0){
      printf("/dev/ttyS0:%s\r\n",tempchar);
    } else {
      printf("/dev/ttyS0 empty\r\n");
    }
    if (get_from_serial_incoming_buffer_everything("/dev/ttyAMA0",tempchar) > 0){
      printf("/dev/ttyAMA0:%s\r\n",tempchar);
    } else {
      printf("/dev/ttyAMA0 empty\r\n");
    }

    if (get_from_serial_incoming_buffer_everything("/dev/ttyS0",tempchar) > 0){
      printf("/dev/ttyS0:%s\r\n",tempchar);
    } else {
      printf("/dev/ttyS0 empty\r\n");
    }
    if (get_from_serial_incoming_buffer_everything("/dev/ttyAMA0",tempchar) > 0){
      printf("/dev/ttyAMA0:%s\r\n",tempchar);
    } else {
      printf("/dev/ttyAMA0 empty\r\n");
    }


    char str[16];

    while(1){

      // send commands to the AVR on /dev/ttyS0 and get responses back

      send_out_serial_port("/dev/ttyS0","p\r");
   
      usleep(100000);

      if (get_from_serial_incoming_buffer_one_line("/dev/ttyS0",tempchar) > 0){
        printf("/dev/ttyS0:%s\r\n",tempchar);
      }

      usleep(100000);

      if (get_from_serial_incoming_buffer_one_line("/dev/ttyS0",tempchar) > 0){
        printf("/dev/ttyS0:%s\r\n",tempchar);
      }
         

      usleep(100000);


    }

  }

#endif //#if defined(TEST_STANDALONE_COMPILE)