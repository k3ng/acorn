/*
	
	acorn tcp server

  Anthony Good, K3NG

  How this all works:

    tcpserver_main_thread is called with pthread_create() with the tcp port number (i.e. 8888)
    and a pointer to a command handler as parms.  When tcpserver_main_thread sees an incoming
    client connection on the port, it launches a tcp_connection_handler which services the incoming 
    data and calls the command handler passed to it from tcpserver_main_thread to process the 
    incoming data.  Whatever string is returned by the command handler is written out the tcp
    socket to the client.



  Standalone compile with:

    gcc -g -o tcpserver debug.c tcpserver.c -pthread

  The standalone compilation turns up listeners on port 8888, 8889, and 8890.  Commands are hi and quit. 

  To test run:

    sudo ./tcpserver

    telnet localhost 8888


*/



#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>	
#include<unistd.h>
#include<pthread.h>
#include "acorn.h"
#include "debug.h"



// ---------------------------------------------------------------------------------------


#if !defined(COMPILING_EVERYTHING)

  int shutdown_flag = 0;

  struct tcpserver_parms_struct{

    int tcpport;
    int (*command_handler)(char *request, char *response);

  };

  struct tcp_connection_handler_parms_struct{

    int client_sock;
    int (*command_handler)(char *request, char *response);

  };  

#else

  #include <fftw3.h>
  #include <complex.h>
  #include "acorn-server.h"
  #include "sdr.h"
  #include "sound.h"

#endif //COMPILING_EVERYTHING

// ---------------------------------------------------------------------------------------


char debug_text[64];



// ---------------------------------------------------------------------------------------


// void *tcp_connection_handler(void *socket_desc){

void *tcp_connection_handler(void *passed_tcp_connection_handler_parms){


  /*

       This handles the connection for each client

  */

  int connection_active = 1;

  struct tcp_connection_handler_parms_struct tcp_connection_handler_parms = *(struct tcp_connection_handler_parms_struct*)passed_tcp_connection_handler_parms;
  int client_sock = tcp_connection_handler_parms.client_sock;

	int read_size;
	char client_message_temp[34], client_message[34], sdr_response[1000], debug_text[100];

  sprintf(debug_text,"tcp_connection_handler: starting client_sock:%d", client_sock);
  debug(debug_text,1);
	
	char *message = "acorn ready!\n";
	write(client_sock, message, strlen(message));


  while(connection_active && !shutdown_flag){
    read_size = recv(client_sock, client_message_temp, 32, 0);
    if (read_size > 0){
      strncpy(client_message,client_message_temp,read_size);
      //echo the message back to client
      // sprintf(debug_text,"tcp_connection_handler: client_sock:%d msg:%s strlen:%d read_size:%d", client_sock, client_message, strlen(client_message), read_size);
      // debug(debug_text,3);
      write(client_sock, client_message, read_size);

      // yank off the carriage return and whatever
      char *return_character = strchr(client_message, '\r');
      if (return_character){
        int number_of_command_characters = return_character - client_message;
        strcpy(client_message_temp,client_message);
        strncpy(client_message, client_message_temp, number_of_command_characters);
        client_message[number_of_command_characters] = 0;
      }


      // handle some telnet commands right here
      if (!strcmp(client_message,"quit")){  
        sprintf(debug_text,"tcp_connection_handler: client quit client_sock: %d", client_sock);
        debug(debug_text,1); 
        close(client_sock);    
        free(passed_tcp_connection_handler_parms);
        return RETURN_NO_ERROR;  
      } else if(!strcmp(client_message,"shutdown")){
        sprintf(client_message,"shutting down!\r\n");
        write(client_sock, client_message, strlen(client_message));  
        sprintf(debug_text,"tcp_connection_handler: client shutdown client_sock: %d", client_sock);
        debug(debug_text,1); 
        close(client_sock);    
        free(passed_tcp_connection_handler_parms);      
        shutdown_flag = 1;
        return RETURN_NO_ERROR;        
      } else if(!strcmp(client_message,"hi")){
        sprintf(client_message,"hi from tcp_connection_handler!\r\n");
        write(client_sock, client_message, strlen(client_message));
      } else {  
        // other commands go to the passed command handler
        #if defined(COMPILING_EVERYTHING)

          tcp_connection_handler_parms.command_handler(client_message, sdr_response); 

          //sprintf(client_message,"%s\r\n",sdr_response);
          // write(client_sock, client_message, strlen(client_message));
          write(client_sock, sdr_response, strlen(sdr_response));
          write(client_sock, "\r\n", 2);
        #endif
      }
      strcpy(sdr_response,"");
      strcpy(client_message_temp,"");
      strcpy(client_message,"");
    } else {
      connection_active = 0;
    }
  }

	
	if(read_size == 0){
    sprintf(debug_text,"tcp_connection_handler: tcp_connection_handler: client disconnected client_sock: %d", client_sock);
    debug(debug_text,1);    
	} else if(read_size == -1){
    sprintf(debug_text,"tcp_connection_handler: tcp_connection_handler: recv failed client_sock: %d", client_sock);
    debug(debug_text,255);       
	}
		

  sprintf(debug_text,"tcp_connection_handler: exiting client_sock:%d", client_sock);
  debug(debug_text,1);


  free(passed_tcp_connection_handler_parms);

	return RETURN_NO_ERROR;
}

// ---------------------------------------------------------------------------------------


void *tcpserver_main_thread(void *passed_tcpserver_parms){

  struct tcpserver_parms_struct tcpserver_parms = *(struct tcpserver_parms_struct*)passed_tcpserver_parms;
	int socket_desc, client_sock, c, *new_sock;
	struct sockaddr_in server, client;
  char ipAddress[INET_ADDRSTRLEN];
  int bind_successful = 0;
  char debug_text[100];
	
  while (!bind_successful){
  	// create socket
  	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  	if (socket_desc == -1){
      sprintf(debug_text,"tcpserver_main_thread: could not create socket");
      debug(debug_text,255);     
      exit;   
  	}
    sprintf(debug_text,"tcpserver_main_thread: socket created: %d", socket_desc);
    debug(debug_text,1);   
  	
  	// prepare the sockaddr_in structure
  	server.sin_family = AF_INET;
  	server.sin_addr.s_addr = INADDR_ANY;
  	server.sin_port = htons(tcpserver_parms.tcpport);
  	
  	// bind
  	if (bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0){
  		//print the error message
      sprintf(debug_text,"tcpserver_main_thread: socket:%d bind failed, retrying",socket_desc);
      debug(debug_text,255);
      sleep(5);
      close(socket_desc);     	
  	} else { 
      bind_successful = 1;
    }
  }

  sprintf(debug_text,"tcpserver_main_thread: socket:%d bind done",socket_desc);
  debug(debug_text,1);   
	
	listen(socket_desc, 3);
	
	// accept an incoming connection
  sprintf(debug_text,"tcpserver_main_thread: waiting for incoming connections socket:%d port:%d",socket_desc, tcpserver_parms.tcpport);
  debug(debug_text,1); 
	c = sizeof(struct sockaddr_in);
	while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ){

    inet_ntop(AF_INET, &(client.sin_addr.s_addr), ipAddress, INET_ADDRSTRLEN);

    sprintf(debug_text,"tcpserver_main_thread: socket:%d port:%d connection accepted from %s", socket_desc, tcpserver_parms.tcpport, ipAddress);
    debug(debug_text,1); 
		
		pthread_t tcp_connection_handler_thread;

    struct tcp_connection_handler_parms_struct *tcp_connection_handler_parms;
    tcp_connection_handler_parms = malloc(sizeof(tcp_connection_handler_parms));
		tcp_connection_handler_parms->client_sock = client_sock;

    #if defined(COMPILING_EVERYTHING)
      // pass along to tcp_connection_handler the pointer to the command handler we were given
      tcp_connection_handler_parms->command_handler = tcpserver_parms.command_handler;
    #endif

    if (pthread_create(&tcp_connection_handler_thread, NULL, tcp_connection_handler, (void*) tcp_connection_handler_parms) < 0) {
      sprintf(debug_text,"tcpserver_main_thread: client_sock:%d port:%d could not create thread", client_sock, tcpserver_parms.tcpport);
      debug(debug_text,255);       
			//return RETURN_NO_ERROR;
		} else {
      sprintf(debug_text,"tcpserver_main_thread: client_sock:%d port:%d tcp_connection_handler launched", client_sock, tcpserver_parms.tcpport);
      debug(debug_text,1);
    }
		
		// TODO: is this needed?
		//pthread_join(sniffer_thread, NULL);

	}
	


}
// ---------------------------------------------------------------------------------------


#if !defined(COMPILING_EVERYTHING)

  int main(int argc, char *argv[]){

    debug_level = 255;

    struct tcpserver_parms_struct *tcpserver_parms;

    tcpserver_parms = malloc(sizeof(tcpserver_parms));

    tcpserver_parms->tcpport = 8888;


    pthread_t tcpserver_thread_1;

    if (pthread_create(&tcpserver_thread_1, NULL, tcpserver_main_thread, (void*) tcpserver_parms)){
      debug("main: could not create tcpserver_main_thread",255);
      return RETURN_ERROR;
    }
  
    sleep(1);

    tcpserver_parms->tcpport = 8889;

    pthread_t tcpserver_thread_2;

    if (pthread_create(&tcpserver_thread_2, NULL, tcpserver_main_thread, (void*) tcpserver_parms)){
      debug("main: could not create tcpserver_main_thread",255);
      return RETURN_ERROR;
    }    

    sleep(1);

    tcpserver_parms->tcpport = 8890;

    pthread_t tcpserver_thread_3;

    if (pthread_create(&tcpserver_thread_3, NULL, tcpserver_main_thread, (void*) tcpserver_parms)){
      debug("main: could not create tcpserver_main_thread",255);
      return RETURN_ERROR;
    }    

    while(1){sleep(30);};
    
  }



#endif //defined(COMPILING_EVERYTHING)

