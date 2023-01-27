/*
	
	Acorn tcp server



standalone test compilation (TEST_STANDALONE_COMPILE)


compile with:

gcc -g -o tcpserver tcpserver.c -pthread


*/

//#define TEST_STANDALONE_COMPILE

#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>	
#include<unistd.h>
#include<pthread.h>

// ---------------------------------------------------------------------------------------


#if defined(TEST_STANDALONE_COMPILE)

  void debug(char *debug_text_in, int debug_text_level){

    printf(debug_text_in);
    printf("\r\n");

  }

#else

  #include "acorn.h"

#endif //TEST_STANDALONE_COMPILE

// ---------------------------------------------------------------------------------------


char debug_text[64];

// ---------------------------------------------------------------------------------------


void *tcp_connection_handler(void *socket_desc){

/*
     This handles the connection for each client

*/

  int connection_active = 1;

	//Get the socket descriptor
	int sock = *(int*)socket_desc;
	int read_size;
	char *message, client_message[32], client_command[32];

  sprintf(debug_text,"tcp_connection_handler: sock: %d", sock);
  debug(debug_text,1);
	
	//Send some messages to the client
	message = "acorn ready!\n";
	write(sock, message, strlen(message));


  while(connection_active){
    read_size = recv(sock, client_message, 32, 0);
    if (read_size > 0){
      sprintf(client_command,"%s",client_message);
      //echo the message back to client
      sprintf(debug_text,"tcp_connection_handler: sock: %d msg: %s strlen: %d read_size: %d", sock, client_message, strlen(client_message), read_size);
      debug(debug_text,3);
      write(sock, client_message, read_size);

      if (read_size > 1){
        client_command[read_size - 2] = 0;  // yank off the carriage return and whatever
      }

      if (!strcmp(client_command,"quit")){
        sprintf(debug_text,"tcp_connection_handler: tcp_connection_handler: client quit sock: %d", sock);
        debug(debug_text,1); 
        close(sock);    
        free(socket_desc);
        return 0;  
      }
      
      strcpy(client_command,"");
      strcpy(client_message,"");
    } else {
      connection_active = 0;
    }
  }

	
	//Receive a message from client
	// while( (read_size = recv(sock, client_message, 32, 0)) > 0 ){
	// 	//echo the message back to client
  //   sprintf(debug_text,"tcp_connection_handler: sock: %d msg: %s strlen: %d read_size: %d", sock, client_message, strlen(client_message), read_size);
  //   debug(debug_text,1);
  //   strncpy(client_command,client_message,read_size - 2);
	// 	write(sock, client_message, read_size /*strlen(client_message)*/);

  //   strncpy(client_command,client_message,read_size - 2);

  //   if (!strcmp(client_command,"quit")){
  //     sprintf(debug_text,"tcp_connection_handler: tcp_connection_handler: client quit sock: %d", sock);
  //     debug(debug_text,1); 
  //     close(sock);    
  //     free(socket_desc);
  //     return 0;  
  //   }
    
  //   strcpy(client_command,"");
  //   strcpy(client_message,"");
   
	// }
	
	if(read_size == 0){
    sprintf(debug_text,"tcp_connection_handler: tcp_connection_handler: client disconnected sock: %d", sock);
    debug(debug_text,1);    
		//fflush(stdout);
	} else if(read_size == -1){
    sprintf(debug_text,"tcp_connection_handler: tcp_connection_handler: recv failed sock: %d", sock);
    debug(debug_text,1);       
	}
		
	//Free the socket pointer
	free(socket_desc);
	
	return 0;
}

// ---------------------------------------------------------------------------------------


void *tcpserver_main_thread(){
	int socket_desc, client_sock, c, *new_sock;
	struct sockaddr_in server, client;
  char ipAddress[INET_ADDRSTRLEN];
  int bind_successful = 0;
	
  while (!bind_successful){
  	// create socket
  	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
  	if (socket_desc == -1){
      sprintf(debug_text,"tcpserver_main_thread: could not create socket");
      debug(debug_text,1);     
      exit;   
  	}
    sprintf(debug_text,"tcpserver_main_thread: socket created: %d", socket_desc);
    debug(debug_text,1);   
  	
  	// prepare the sockaddr_in structure
  	server.sin_family = AF_INET;
  	server.sin_addr.s_addr = INADDR_ANY;
  	server.sin_port = htons(8888);
  	
  	// bind
  	if (bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0){
  		//print the error message
      sprintf(debug_text,"tcpserver_main_thread: bind failed, retrying");
      debug(debug_text,1);
      sleep(5);
      close(socket_desc);     	
  	} else { 
      bind_successful = 1;
    }
  }

  sprintf(debug_text,"tcpserver_main_thread: bind done");
  debug(debug_text,1);   
	
	listen(socket_desc, 3);
	
	// accept an incoming connection
  sprintf(debug_text,"tcpserver_main_thread: waiting for incoming connections...");
  debug(debug_text,1); 
	c = sizeof(struct sockaddr_in);
	while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ){

    inet_ntop(AF_INET, &(client.sin_addr.s_addr), ipAddress, INET_ADDRSTRLEN);

    sprintf(debug_text,"tcpserver_main_thread: connection accepted from %s", ipAddress);
    debug(debug_text,1); 
		
		pthread_t sniffer_thread;
		new_sock = malloc(1);
		*new_sock = client_sock;
		
		if (pthread_create( &sniffer_thread, NULL, tcp_connection_handler, (void*) new_sock) < 0){
      sprintf(debug_text,"tcpserver_main_thread: could not create thread");
      debug(debug_text,1);       
			return 0;
		}
		
		//Now join the thread , so that we dont terminate before the thread
		// pthread_join( sniffer_thread , NULL);
		// puts("tcpserver_main_thread: handler assigned");
	}
	
	if (client_sock < 0){
    sprintf(debug_text,"tcpserver_main_thread: accept failed");
    debug(debug_text,1);           
		return 0;
	}
	

}
// ---------------------------------------------------------------------------------------


#if defined(TEST_STANDALONE_COMPILE)

  int main(int argc, char *argv[]){

    pthread_t main_thread;

    if (pthread_create(&main_thread, NULL, tcpserver_main_thread, NULL)){
      perror("main: could not create main thread");
      return 1;
    }

    while(1){sleep(30);};
    
  }

#endif //defined(TEST_STANDALONE_COMPILE)

