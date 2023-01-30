#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <wiringSerial.h>
#include <termios.h>
#include <signal.h>
#include <curses.h>

int fd;
char serial_byte[2];
unsigned char c[32];
int r;
struct termios t;
int exit_flag = 0;



void signal_handler(int sig){

  signal(sig, SIG_IGN);
  printf("exiting...\n");
  // tcgetattr(0,&t);
  // t.c_lflag |= ICANON;
  // tcsetattr(0,0,&t);
  exit_flag = 1;

}


void main(){

  signal(SIGINT, signal_handler);

    
  // change stdin to non-blocking input
  // tcgetattr(0,&t);
  // t.c_lflag &= ~ICANON;
  // tcsetattr(0,0,&t);

	fd = serialOpen("/dev/ttyS0", 115200);
	sleep(1);

  printf("fd: %d\r\n",fd);

  //serialPrintf(fd, "v\r");

  initscr();
  timeout(1);

	while(!exit_flag){

    r = getch();

    //if ((r = read(0, &c, sizeof(c))) >= 0) {
    if (r != ERR){
      c[0] = r;
      c[1] = 0;
      printf("%s",c);
      serialPrintf(fd, c);
    }

  	if (serialDataAvail(fd)){
  	  serial_byte[0] = serialGetchar(fd);
      serial_byte[1] = 0;
      fprintf(stdout,serial_byte);
  	}

  	usleep(1000);

  }

  endwin();

}