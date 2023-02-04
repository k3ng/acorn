/*
	
	acorn tcp server

  Anthony Good, K3NG




  standalone compile with:

  gcc -g -o tcpserver tcpserver.c -pthread


*/



#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>	
#include<unistd.h>
#include<pthread.h>



// ---------------------------------------------------------------------------------------


#if !defined(COMPILING_EVERYTHING)

  int shutdown_flag = 0;

  void debug(char *debug_text_in, int debug_text_level){

    printf(debug_text_in);
    printf("\r\n");
    fflush(stdout);

  }

  struct command_handler_struct{

    char *request;
    char *response;

  };


  struct tcpserver_struct{

    int tcpport;
    int (*command_handler)(struct command_handler_struct *passed_request_and_response);

  };

#else

  #include <fftw3.h>
  #include <complex.h>
  #include "acorn.h"
  #include "sdr.h"
  #include "sound.h"

#endif //COMPILING_EVERYTHING

// ---------------------------------------------------------------------------------------


char debug_text[64];

struct tcp_connection_handler_struct{

  int socket_desc;
  int (*command_handler)(struct command_handler_struct *passed_request_and_response);

};

// ---------------------------------------------------------------------------------------


void *tcp_connection_handler(void *socket_desc){

// void *tcp_connection_handler(void *passed_tcp_connection_handler_struct){



/*
     This handles the connection for each client

*/

  int connection_active = 1;

	//Get the socket descriptor
	int sock = *(int*)socket_desc;
  // struct tcp_connection_handler_struct new_tcp_connection_handler_struct = *(struct tcp_connection_handler_struct*)passed_tcp_connection_handler_struct;
  // int sock = new_tcp_connection_handler_struct.socket_desc;

	int read_size;
	char *message, client_message[32], sdr_response[32];
  char debug_text[100];

  sprintf(debug_text,"tcp_connection_handler: starting sock:%d", sock);
  debug(debug_text,1);
	
	message = "acorn ready!\n";
	write(sock, message, strlen(message));


  while(connection_active && !shutdown_flag){
    read_size = recv(sock, client_message, 32, 0);
    if (read_size > 0){
      //echo the message back to client
      sprintf(debug_text,"tcp_connection_handler: sock: %d msg: %s strlen: %d read_size: %d", sock, client_message, strlen(client_message), read_size);
      debug(debug_text,3);
      write(sock, client_message, read_size);

      if (read_size > 1){
        client_message[read_size - 2] = 0;  // yank off the carriage return and whatever
      }


      // handle some telnet commands right here  TODO: tie in a command handler
      if (!strcmp(client_message,"quit")){  
        sprintf(debug_text,"tcp_connection_handler: tcp_connection_handler: client quit sock: %d", sock);
        debug(debug_text,1); 
        close(sock);    
        free(socket_desc);
        // free(passed_tcp_connection_handler_struct);
        return 0;  
      } else {  
        // other commands go to SDR
        #if defined(COMPILING_EVERYTHING)
          sdr_request(client_message, sdr_response); 
          sprintf(client_message,"%s\r\n",sdr_response);
          write(sock, client_message, strlen(client_message));
        #endif
      }
      

      strcpy(client_message,"");
    } else {
      connection_active = 0;
    }
  }

	
	if(read_size == 0){
    sprintf(debug_text,"tcp_connection_handler: tcp_connection_handler: client disconnected sock: %d", sock);
    debug(debug_text,1);    
	} else if(read_size == -1){
    sprintf(debug_text,"tcp_connection_handler: tcp_connection_handler: recv failed sock: %d", sock);
    debug(debug_text,255);       
	}
		

  sprintf(debug_text,"tcp_connection_handler: exiting sock:%d", sock);
  debug(debug_text,1);

	//Free the socket pointer
	free(socket_desc);
	// free(sock);
  // free(passed_tcp_connection_handler_struct);

	return 0;
}

// ---------------------------------------------------------------------------------------


void *tcpserver_main_thread(void *passed_tcpserver_struct){

  struct tcpserver_struct new_tcpserver_struct = *(struct tcpserver_struct*)passed_tcpserver_struct;
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
  	server.sin_port = htons(new_tcpserver_struct.tcpport);
  	
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
  sprintf(debug_text,"tcpserver_main_thread: waiting for incoming connections socket:%d port:%d",socket_desc, new_tcpserver_struct.tcpport);
  debug(debug_text,1); 
	c = sizeof(struct sockaddr_in);
	while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ){

    inet_ntop(AF_INET, &(client.sin_addr.s_addr), ipAddress, INET_ADDRSTRLEN);

    sprintf(debug_text,"tcpserver_main_thread: socket:%d port:%d connection accepted from %s", socket_desc, new_tcpserver_struct.tcpport, ipAddress);
    debug(debug_text,1); 
		
		pthread_t tcp_connection_handler_thread;
		new_sock = malloc(1);
		*new_sock = client_sock;
    // struct tcp_connection_handler_struct *new_tcp_connection_handler_struct;
    // new_tcp_connection_handler_struct = malloc(sizeof(new_tcp_connection_handler_struct));

		// new_tcp_connection_handler_struct->socket_desc = socket_desc;

		if (pthread_create(&tcp_connection_handler_thread, NULL, tcp_connection_handler, (void*) new_sock) < 0) {
    //if (pthread_create(&sniffer_thread, NULL, tcp_connection_handler, (void*) new_tcp_connection_handler_struct) < 0) {
      sprintf(debug_text,"tcpserver_main_thread: client_sock:%d port:%d could not create thread", client_sock, new_tcpserver_struct.tcpport);
      debug(debug_text,255);       
			//return 0;
		} else {
      sprintf(debug_text,"tcpserver_main_thread: client_sock:%d port:%d tcp_connection_handler launched", client_sock, new_tcpserver_struct.tcpport);
      debug(debug_text,1);
    }
		
		// TODO: is this needed?
		//pthread_join(sniffer_thread, NULL);

    sprintf(debug_text,"tcpserver_main_thread: handler assigned");
    debug(debug_text,2);     
	}
	
	if (client_sock < 0){
    sprintf(debug_text,"tcpserver_main_thread: accept failed");
    debug(debug_text,255);           
		return 0;
	}
	

}
// ---------------------------------------------------------------------------------------


#if !defined(COMPILING_EVERYTHING)

  int main(int argc, char *argv[]){

    struct tcpserver_struct *new_tcpserver;

    new_tcpserver = malloc(sizeof(new_tcpserver));

    new_tcpserver->tcpport = 8888;



    pthread_t tcpserver_thread_1;

    if (pthread_create(&tcpserver_thread_1, NULL, tcpserver_main_thread, (void*) new_tcpserver)){
      debug("main: could not create tcpserver_main_thread",255);
      return 1;
    }

    // new_tcpserver->tcpport = 8889;

    // pthread_t tcpserver_thread_2;

    // if (pthread_create(&tcpserver_thread_2, NULL, tcpserver_main_thread, (void*) new_tcpserver)){
    //   debug("main: could not create tcpserver_main_thread",255);
    //   return 1;
    // }    

    // new_tcpserver->tcpport = 8890;

    // pthread_t tcpserver_thread_3;

    // if (pthread_create(&tcpserver_thread_3, NULL, tcpserver_main_thread, (void*) new_tcpserver)){
    //   debug("main: could not create tcpserver_main_thread",255);
    //   return 1;
    // }    

    while(1){sleep(30);};
    
  }



#endif //defined(COMPILING_EVERYTHING)

