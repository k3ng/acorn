#include <stdio.h> /* Standard I/O Definition*/  
#include <stdlib.h> /* Standard Function Library Definition*/  
#include <unistd.h> /*Unix Standard Function Definition*/  
#include <sys/types.h>   
#include <sys/stat.h>     
#include <fcntl.h> /* File Control Definition*/  
#include <termios.h> /*PPSIX Terminal Control Definition*/  
#include <errno.h> /* Error Number Definition*/  
#include <string.h>  
   
   
//Macro Definition  
#define FALSE  -1  
#define TRUE   0  
   
/******************************************************************* 
* Name: * UART0_Open 
* Function: Open the serial port and return the description of the serial device file 
* Entry parameter: fd File descriptor port: serial password (ttyS0,ttyS1,ttyS2) 
* Export parameters:) Correct return to 1, error return to 0 
*******************************************************************/  
int UART0_Open(int fd,char* port)  
{  
     
         fd = open( port, O_RDWR|O_NOCTTY|O_NDELAY);  
         if (FALSE == fd)  
                {  
                       perror("Can't Open Serial Port");  
                       return(FALSE);  
                }  
     //Restore the serial port to blocked state.  
     if(fcntl(fd, F_SETFL, 0) < 0)  
                {  
                       printf("fcntl failed!\n");  
                     return(FALSE);  
                }       
         else  
                {  
                  printf("fcntl=%d\n",fcntl(fd, F_SETFL,0));  
                }  
      //Test whether it is a terminal device or not.  
      if(0 == isatty(STDIN_FILENO))  
                {  
                       printf("standard input is not a terminal device\n");  
                  return(FALSE);  
                }  
  else  
                {  
                     printf("isatty success!\n");  
                }                
  printf("fd->open=%d\n",fd);  
  return fd;  
}  
/******************************************************************* 
* Name: * UART0_Close 
* Function: Close the serial port and return the description of the serial device file 
* Entry parameter: fd File descriptor port: serial password (ttyS0,ttyS1,ttyS2) 
* Export parameters: void 
*******************************************************************/  
   
void UART0_Close(int fd)  
{  
    close(fd);  
}  
   
/******************************************************************* 
* Name: * UART0_Set 
* Function: Set up serial data bits, stop bits and validation bits 
* Entry parameters: fd Serial port file descriptor 
*                              speed     Serial port speed 
*                              flow_ctrl   Data flow control 
*                           databits   Data Bit: Value 7 or 8 
*                           stopbits   Stop Bit: Value 1 or 2 
*                           parity     Value of validation type N, E, O, S 
*Export parameters:) Correct return to 1, error return to 0 
*******************************************************************/  
int UART0_Set(int fd,int speed,int flow_ctrl,int databits,int stopbits,int parity)  
{  
     
      int   i;  
         int   status;  
         int   speed_arr[] = { B115200, B19200, B9600, B4800, B2400, B1200, B300};  
     int   name_arr[] = {115200,  19200,  9600,  4800,  2400,  1200,  300};  
           
    struct termios options;  
     
    /*tcgetattr(fd,&options)The function can also test whether the configuration is correct, whether the serial port is available and so on. If the call is successful, the return value of the function is 0, and if the call fails, the return value of the function is 1. 
    */  
    if  ( tcgetattr( fd,&options)  !=  0)  
       {  
          perror("SetupSerial 1");      
          return(FALSE);   
       }  
    
    //Setting Serial Port Input and Output Baud Rates  
    for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++)  
                {  
                     if  (speed == name_arr[i])  
                            {               
                                 cfsetispeed(&options, speed_arr[i]);   
                                 cfsetospeed(&options, speed_arr[i]);    
                            }  
              }       
     
    //Modify the control mode to ensure that the program does not occupy the serial port  
    options.c_cflag |= CLOCAL;  
    //Modify the control mode so that input data can be read from serial port  
    options.c_cflag |= CREAD;  
    
    //Setting up data flow control  
    switch(flow_ctrl)  
    {  
        
       case 0 ://No flow control  
              options.c_cflag &= ~CRTSCTS;  
              break;     
        
       case 1 ://Using Hardware Flow Control  
              options.c_cflag |= CRTSCTS;  
              break;  
       case 2 ://Using Software Flow Control  
              options.c_cflag |= IXON | IXOFF | IXANY;  
              break;  
    }  
    //set data bit  
    //Shielding other markers  
    options.c_cflag &= ~CSIZE;  
    switch (databits)  
    {    
       case 5    :  
                     options.c_cflag |= CS5;  
                     break;  
       case 6    :  
                     options.c_cflag |= CS6;  
                     break;  
       case 7    :      
                 options.c_cflag |= CS7;  
                 break;  
       case 8:      
                 options.c_cflag |= CS8;  
                 break;    
       default:     
                 fprintf(stderr,"Unsupported data size\n");  
                 return (FALSE);   
    }  
    //Setting Check Bit  
    switch (parity)  
    {    
       case 'n':  
       case 'N': //No parity bits.  
                 options.c_cflag &= ~PARENB;   
                 options.c_iflag &= ~INPCK;      
                 break;   
       case 'o':    
       case 'O'://Set it to odd check.  
                 options.c_cflag |= (PARODD | PARENB);   
                 options.c_iflag |= INPCK;               
                 break;   
       case 'e':   
       case 'E'://Set to Dual Check  
                 options.c_cflag |= PARENB;         
                 options.c_cflag &= ~PARODD;         
                 options.c_iflag |= INPCK;        
                 break;  
       case 's':  
       case 'S': //Set to blank  
                 options.c_cflag &= ~PARENB;  
                 options.c_cflag &= ~CSTOPB;  
                 break;   
        default:    
                 fprintf(stderr,"Unsupported parity\n");      
                 return (FALSE);   
    }   
    //Setting stop bits  
    switch (stopbits)  
    {    
       case 1:     
                 options.c_cflag &= ~CSTOPB; break;   
       case 2:     
                 options.c_cflag |= CSTOPB; break;  
       default:     
                       fprintf(stderr,"Unsupported stop bits\n");   
                       return (FALSE);  
    }  
     
  //Modify the output mode to output raw data  
  options.c_oflag &= ~OPOST;  
    
  options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);//I added  
//options.c_lflag &= ~(ISIG | ICANON);  
     
    //Set waiting time and minimum receive character  
    options.c_cc[VTIME] = 1; /* Read a character and wait for 1*(1/10)s*/    
    options.c_cc[VMIN] = 1; /* The minimum number of characters read is 1.*/  
     
    //If data overflow occurs, receive data, but no longer read, refresh received data, but do not read  
    tcflush(fd,TCIFLUSH);  
     
    //Activation configuration (set the modified termios data to the serial port)  
    if (tcsetattr(fd,TCSANOW,&options) != 0)    
           {  
               perror("com set error!\n");    
              return (FALSE);   
           }  
    return (TRUE);   
}  
/******************************************************************* 
* Name: * UART0_Init() 
* Function: Serial Initialization 
* Entry parameter: fd File descriptor 
*               speed  :  Serial port speed 
*                              flow_ctrl  Data flow control 
*               databits   Data Bit: Value 7 or 8 
*                           stopbits   Stop Bit: Value 1 or 2 
*                           parity     Value of validation type N, E, O, S 
*                       
* Export parameters:) Correct return to 1, error return to 0 
*******************************************************************/  
int UART0_Init(int fd, int speed,int flow_ctrl,int databits,int stopbits,int parity)  
{  
    int err;  
    //Setting Serial Data Frame Format  
    if (UART0_Set(fd,19200,0,8,1,'N') == FALSE)  
       {                                                           
        return FALSE;  
       }  
    else  
       {  
               return  TRUE;  
        }  
}  
   
/******************************************************************* 
* Name: * UART0_Recv 
* Function: Receiving Serial Data 
* Entry parameter: fd File descriptor 
*                              rcv_buf     :Data stored in rcv_buf buffer in receiving serial port 
*                              data_len    :Length of a frame of data 
* Export parameters:) Correct return to 1, error return to 0 
*******************************************************************/  
int UART0_Recv(int fd, char *rcv_buf,int data_len)  
{  
    int len,fs_sel;  
    fd_set fs_read;  
     
    struct timeval time;  
     
    FD_ZERO(&fs_read);  
    FD_SET(fd,&fs_read);  
     
    time.tv_sec = 10;  
    time.tv_usec = 0;  
     
    //Using select to realize multi-channel communication of serial port  
    fs_sel = select(fd+1,&fs_read,NULL,NULL,&time);  
    if(fs_sel)  
       {  
              len = read(fd,rcv_buf,data_len);  
          printf("I am right!(version1.2) len = %d fs_sel = %d\n",len,fs_sel);  
              return len;  
       }  
    else  
       {  
          printf("Sorry,I am wrong!");  
              return FALSE;  
       }       
}  
/******************************************************************** 
* Name: UART0_Send 
* Function: Send data 
* Entry parameter: fd File descriptor 
*                              send_buf    :Store serial port to send data 
*                              data_len    :Number of data per frame 
* Export parameters:) Correct return to 1, error return to 0 
*******************************************************************/  
int UART0_Send(int fd, char *send_buf,int data_len)  
{  
    int len = 0;  
     
    len = write(fd,send_buf,data_len);  
    if (len == data_len )  
              {  
                     return len;  
              }       
    else     
        {  
                 
                tcflush(fd,TCOFLUSH);  
                return FALSE;  
        }  
     
}  
   
   
int main(int argc, char **argv)  
{  
    int fd;                            //File descriptor  
    int err;                           //Returns the state of the calling function  
    int len;                          
    int i;  
    char rcv_buf[100];         
    char send_buf[20]="v";  
    if(argc != 3)  
       {  
              printf("Usage: %s /dev/ttySn 0(send data)/1 (receive data) \n",argv[0]);  
              return FALSE;  
       }  
    fd = UART0_Open(fd,argv[1]); //Open the serial port and return the file descriptor  
    do{  
                  err = UART0_Init(fd,115200,0,8,1,'N');  
                  printf("Set Port Exactly!\n");  
       }while(FALSE == err || FALSE == fd);  
     
    if(0 == strcmp(argv[2],"0"))  
           {  
                  for(i = 0;i < 10;i++)  
                         {  
                                len = UART0_Send(fd,send_buf,10);  
                                if(len > 0)  
                                       printf(" %d send data successful\n",i);  
                                else  
                                       printf("send data failed!\n");  
                            
                                sleep(2); 

                     len = UART0_Recv(fd, rcv_buf,9);  
                     if(len > 0)  
                            {  
                       rcv_buf[len] = '\0';  
                                   printf("receive data is %s\n",rcv_buf);  
                       printf("len = %d\n",len);  
                            }  
                     else  
                            {  
                                   printf("cannot receive data\n");  
                            }  
                     sleep(2);  


                                 
                         }  
                  UART0_Close(fd);               
           }  
    else  
           {  
                                        
           while (1) //Loop read data  
                  {    
                     len = UART0_Recv(fd, rcv_buf,9);  
                     if(len > 0)  
                            {  
                       rcv_buf[len] = '\0';  
                                   printf("receive data is %s\n",rcv_buf);  
                       printf("len = %d\n",len);  
                            }  
                     else  
                            {  
                                   printf("cannot receive data\n");  
                            }  
                     sleep(2);  
              }              
       UART0_Close(fd);   
           }  
}  