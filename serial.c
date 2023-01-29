#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>


int fd = 0;
char str[50];

int serial_port_set_interface_attribs (int fd, int speed, int parity, int should_block){

  struct termios tty;
  if (tcgetattr (fd, &tty) != 0){
    fprintf(stdout,"error %d from tcgetattr", errno);
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
    fprintf(stdout,"error %d from tcsetattr", errno);
    return -1;
  }


  tty.c_cc[VMIN]  = should_block ? 1 : 0;
  tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

  if (tcsetattr (fd, TCSANOW, &tty) != 0){
    fprintf(stdout,"error %d setting term attributes", errno);
  }


  return 0;
}

// void serial_port_set_blocking (int fd, int should_block){

//   struct termios tty;
//   memset (&tty, 0, sizeof tty);
//   if (tcgetattr (fd, &tty) != 0){
//     fprintf(stdout,"error %d from tggetattr", errno);
//     return;
//   }

//   tty.c_cc[VMIN]  = should_block ? 1 : 0;
//   tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout

//   if (tcsetattr (fd, TCSANOW, &tty) != 0){
//     fprintf(stdout,"error %d setting term attributes", errno);
//   }

// }


void main(){

  char *portname = "/dev/ttyS0";

  fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
  if (fd < 0){
    fprintf(stdout,"error %d opening %s: %s", errno, portname, strerror (errno));
    return;
  }

  serial_port_set_interface_attribs (fd, B115200, 0, 0);  // set speed to 115,200 bps, 8n1 (no parity)
  //serial_port_set_blocking (fd, 0);                // set no blocking

  // write (fd, "\r\nv\r\n", 5);           // send 7 character greeting

  // usleep ((5 + 25) * 100);             // sleep enough to transmit the 7 plus
  //                                      // receive 25:  approx 100 uS per char transmit
  char buf [100];

  while(1){
    fgets(str, 50, stdin);
    strcat(str,"\r\n");
    write (fd, str, strlen(str));
    usleep ((strlen(str) + 25) * 100);
    int n = read (fd, buf, sizeof buf);  // read up to 100 characters if ready to read
    if (n){
      puts(buf);
    }
    sleep (1);
  }

}