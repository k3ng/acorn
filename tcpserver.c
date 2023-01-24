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

	//Get the socket descriptor
	int sock = *(int*)socket_desc;
	int read_size;
	char *message , client_message[2000];

  sprintf(debug_text,"tcp_connection_handler: sock: %d", sock);
  debug(debug_text,1);
	
	//Send some messages to the client
	message = "Greetings! I am your connection handler\n";
	write(sock , message , strlen(message));
	
	message = "Now type something and i shall repeat what you type \n";
	write(sock , message , strlen(message));
	
	//Receive a message from client
	while( (read_size = recv(sock , client_message , 2000 , 0)) > 0 ){
		//Send the message back to client
		write(sock , client_message , strlen(client_message));
	}
	
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
	
	// create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
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
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0){
		//print the error message
    sprintf(debug_text,"tcpserver_main_thread: bind failed");
    debug(debug_text,1);     
		exit;
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
			exit;
		}
		
		//Now join the thread , so that we dont terminate before the thread
		// pthread_join( sniffer_thread , NULL);
		// puts("tcpserver_main_thread: handler assigned");
	}
	
	if (client_sock < 0){
    sprintf(debug_text,"tcpserver_main_thread: accept failed");
    debug(debug_text,1);           
		exit;
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

