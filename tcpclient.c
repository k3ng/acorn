/*


  Standalone compile:

    gcc -g -o tcpclient debug.c tcpclient.c -pthread

*/

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <complex.h>
#include <ctype.h>
#include <sys/time.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "acorn.h"
#include "debug.h"
#include "tcpclient.h"


char debug_text[64];

#if !defined(COMPILING_EVERYTHING)
  int connection_handle[MAX_TCPCLIENTS+5];
#endif



// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------


long get_address(char *host){

	int i, dotcount=0;
	char *p = host;
	struct hostent		*pent;
	/* struct sockaddr_in	addr; */ /* see the note on portabilit at the end of the routine */

  // does this look like an IP address?
	while (*p){
		for (i = 0; i < 3; i++, p++)
			if (!isdigit(*p))
				break;
		if (*p != '.')
			break;
		p++;
		dotcount++;
	}
  // TODO: check if each octet >= 0 and =< 255 
	if (dotcount == 3 && i > 0 && i <= 3){
		return inet_addr(host);
	}

	sprintf(debug_text,"get_address: looking up %s", host);
  debug(debug_text,2);
	pent = gethostbyname(host);
	if (!pent){
		sprintf("get_address: failed to resolve %s", host);	
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return 0;
	}

	/* PORTABILITY ISSUE: replacing a costly memcpy call with a hack, may not work on 
	some systems.  
	memcpy(&addr.sin_addr, (pent->h_addr), pent->h_length);
	return addr.sin_addr.s_addr; */

	return *((long *)(pent->h_addr));
}

// ---------------------------------------------------------------------------------------


void *tcpclient_thread_function(void *passed_tcpclient_parms){


  struct tcpclient_parms_struct tcpclient_parms = *(struct tcpclient_parms_struct*)passed_tcpclient_parms;

  struct sockaddr_storage serverStorage;
  socklen_t addr_size;
	struct sockaddr_in serverAddr;
	char buff[200], s_name[100];
	int tcp_sock = 0;


	strcpy(s_name, tcpclient_parms.server); 

	if (strlen(tcpclient_parms.server) > sizeof(s_name) - 1)
		return NULL;

	char *host_name = strtok((char *)s_name, ":");
	char *port = strtok(NULL, "");

	if(!host_name){
		sprintf(debug_text,"tcpclient_thread_function: invalid hostname:%s",host_name);
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return NULL;
	}
	if(!port){
		sprintf(debug_text,"tcpclient_thread_function: invalid port:%s",port);
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return NULL;	
	}	

	sprintf(debug_text,"tcpclient_thread_function: finding %s:%s", host_name, port);
	debug(debug_text,1);

  memset(serverAddr.sin_zero, '\0', sizeof serverAddr.sin_zero);  
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(atoi(port));
  serverAddr.sin_addr.s_addr = get_address(host_name); 

	sprintf(debug_text,"tcpclient_thread_function: opening %s:%s", host_name, port);
	debug(debug_text,1);

	tcp_sock = socket(AF_INET, SOCK_STREAM, 0);

  tcpclient_handle_tcp_sock_array[tcpclient_parms.tcpclient_handle] = tcp_sock;


  if (connect(tcp_sock,(struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
		sprintf(debug_text,"tcpclient_thread_function: failed to connect to %s", host_name);
	  debug(debug_text,DEBUG_LEVEL_STDERR);
		close(tcp_sock);

    tcpclient_handle_tcp_sock_array[tcpclient_parms.tcpclient_handle] = -1;
		return NULL;
   }

	int e;
	while((e = recv(tcp_sock, buff, sizeof(buff), 0)) >= 0){
		if (e > 0){
			buff[e] = 0;

		  sprintf(debug_text,"tcpclient_thread_function: tcpclient_handle:%d received:%s", tcpclient_parms.tcpclient_handle,buff);
	    debug(debug_text,3);

      // TODO: put in a buffer
			printf(buff);


		}
	}
	close(tcp_sock);
  tcpclient_handle_tcp_sock_array[tcpclient_parms.tcpclient_handle] = -1;	

}

// ---------------------------------------------------------------------------------------


int tcpclient_write(int tcpclient_handle, char *text){


  int tcp_sock = 0;

	if ((tcpclient_handle < 1) || (tcpclient_handle >= MAX_TCPCLIENTS)) {
		sprintf(debug_text,"tcpclient_write: invalid tcpclient_handle:%d", tcpclient_handle);
	  debug(debug_text,DEBUG_LEVEL_STDERR);		
		return -1;
	}

  tcp_sock = tcpclient_handle_tcp_sock_array[tcpclient_handle];

	if (tcp_sock < 1){
		sprintf(debug_text,"tcpclient_write: tcpclient_handle:%d connection is closed", tcpclient_handle);
	  debug(debug_text,DEBUG_LEVEL_STDERR);		
		return -1;
	}

	sprintf(debug_text,"tcpclient_write: tcpclient_handle:%d sending:%s", tcpclient_handle, text);
  debug(debug_text,1);	
	send (tcp_sock, text, strlen(text), 0);

}

// ---------------------------------------------------------------------------------------


int tcpclient_close(int tcpclient_handle){

  int tcp_sock = 0;

	sprintf(debug_text,"tcpclient_close: closing tcpclient_handle:%d",tcpclient_handle);
	debug(debug_text,2);


	if ((tcpclient_handle < 1) || (tcpclient_handle >= MAX_TCPCLIENTS)) {
		sprintf(debug_text,"tcpclient_close: invalid tcpclient_handle:%d", tcpclient_handle);
	  debug(debug_text,DEBUG_LEVEL_STDERR);		
		return -1;
	}

  tcp_sock = tcpclient_handle_tcp_sock_array[tcpclient_handle];

	if (tcp_sock < 1){
		sprintf(debug_text,"tcpclient_close: invalid tcpclient_handle:%d connection already closed", tcpclient_handle);
	  debug(debug_text,DEBUG_LEVEL_STDERR);		
		return -1;
	}

	close(tcp_sock);
	tcpclient_handle_tcp_sock_array[tcpclient_handle] = -1; // -1 = disconnected slot

}



// ---------------------------------------------------------------------------------------


int tcpclient_open(char *server){


  static int run_once = 0;

  if (!run_once){
	  for (int x = 0;x<MAX_TCPCLIENTS;x++){
	  	tcpclient_handle_tcp_sock_array[x] = -1;
	  }
  	run_once = 1;
  }

  struct tcpclient_parms_struct *tcpclient_parms;

  // find an open slot
  int x = 1;
  int assigned_slot = 0;

  for (x = 1;x < MAX_TCPCLIENTS-1;x++){
  	if (tcpclient_handle_tcp_sock_array[x] == -1){
  		assigned_slot = x;
      x = MAX_TCPCLIENTS;
  	}
  }

  if (assigned_slot == 0){
			sprintf(debug_text,"tcpclient_open: out of tcpclient_handles");
			debug(debug_text,DEBUG_LEVEL_STDERR);
		  return -1;	  	
  }

  tcpclient_parms = malloc(sizeof(tcpclient_parms));
  tcpclient_parms->server = server;
  tcpclient_parms->tcpclient_handle = assigned_slot;

  tcpclient_handle_tcp_sock_array[assigned_slot] = -2; // -2 means connecting

	sprintf(debug_text,"tcpclient_open: launching thread for:%s tcpclient_handle:%d",tcpclient_parms->server,tcpclient_parms->tcpclient_handle);
	debug(debug_text,2);  

  pthread_t tcpclient_thread;

 	pthread_create(&tcpclient_thread, NULL, tcpclient_thread_function, (void*)tcpclient_parms);

 	return assigned_slot;

}

// ---------------------------------------------------------------------------------------


int tcpclient_connected(int tcpclient_handle){


  if ((tcpclient_handle > (MAX_TCPCLIENTS-1)) || (tcpclient_handle < 1)){
    return -1;
  }

  if (tcpclient_handle_tcp_sock_array[tcpclient_handle] > 0){
  	return 1;
  }

  return 0;


}

// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------


#if !defined(COMPILING_EVERYTHING)

	int main(int argc, char *argv[]){

		for (int x = 0;x < (MAX_TCPCLIENTS+5);x++) {
      connection_handle[x] = 0;
		}
    
    debug_level = 255;


    // fire up a bunch of connections

    int temp;

    for (int x = 0;x < (MAX_TCPCLIENTS-1);x++) {
	    temp = tcpclient_open(argv[1]);
	    connection_handle[x] = temp;
	    if (connection_handle[x] > 0){
			  while(!tcpclient_connected(connection_handle[x])){
				  usleep(10000);	
			  }
			} else {
				x = 255;
			}
		}    

    for (int x = 0;x < (MAX_TCPCLIENTS+5);x++){
    	if (connection_handle[x] > 0){
        tcpclient_write(connection_handle[x],"hello\r");
        usleep(50000);  
      }
		}
			

    for (int x = 0;x < (MAX_TCPCLIENTS+5);x++){
    	if (connection_handle[x] > 0){
        tcpclient_write(connection_handle[x],"help\r");
        usleep(50000);  
      }
		}			

		
    for (int x = 0;x < (MAX_TCPCLIENTS+5);x++){
    	if (connection_handle[x] > 0){
        tcpclient_close(connection_handle[x]);
      }
		}  


		sleep(3);
	}

#endif
