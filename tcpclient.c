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
// #include "sdr_ui.h"
#include "debug.h"
#include "tcpclient.h"


static int tcpclient_slot_tcp_sock_array[MAX_TCPCLIENTS];

char debug_text[64];

struct tcpclient_parms_struct{

	char *server;
	int tcpclient_slot;

};





// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------


long get_address(char *host)
{
	int i, dotcount=0;
	char *p = host;
	struct hostent		*pent;
	/* struct sockaddr_in	addr; */ /* see the note on portabilit at the end of the routine */

	/*try understanding if this is a valid ip address
	we are skipping the values of the octets specified here.
	for instance, this code will allow 952.0.320.567 through*/
	while (*p)
	{
		for (i = 0; i < 3; i++, p++)
			if (!isdigit(*p))
				break;
		if (*p != '.')
			break;
		p++;
		dotcount++;
	}

	/* three dots with upto three digits in before, between and after ? */
	if (dotcount == 3 && i > 0 && i <= 3)
		return inet_addr(host);

	/* try the system's own resolution mechanism for dns lookup:
	 required only for domain names.
	 inspite of what the rfc2543 :D Using SRV DNS Records recommends,
	 we are leaving it to the operating system to do the name caching.

	 this is an important implementational issue especially in the light
	 dynamic dns servers like dynip.com or dyndns.com where a dial
	 ip address is dynamically assigned a sub domain like farhan.dynip.com

	 although expensive, this is a must to allow OS to take
	 the decision to expire the DNS records as it deems fit.
	*/

	sprintf(debug_text,"get_address: looking up %s", host);
  debug(debug_text,2);
	pent = gethostbyname(host);
	if (!pent){
		sprintf("get_address: failed to resolve %s", host);	
		debug(debug_text,255);
		return 0;
	}

	/* PORTABILITY-ISSUE: replacing a costly memcpy call with a hack, may not work on 
	some systems.  
	memcpy(&addr.sin_addr, (pent->h_addr), pent->h_length);
	return addr.sin_addr.s_addr; */
	return *((long *)(pent->h_addr));
}

// ---------------------------------------------------------------------------------------


// void *tcpclient_thread_function(void *server){
void *tcpclient_thread_function(void *passed_tcpclient_parms){


  struct tcpclient_parms_struct tcpclient_parms = *(struct tcpclient_parms_struct*)passed_tcpclient_parms;

  struct sockaddr_storage serverStorage;
  socklen_t addr_size;
	struct sockaddr_in serverAddr;
	char buff[200], s_name[100];
	int tcp_sock = 0;



	// strcpy(s_name, server); 
	strcpy(s_name, tcpclient_parms.server); 

	if (strlen(tcpclient_parms.server) > sizeof(s_name) - 1)
		return NULL;

	char *host_name = strtok((char *)s_name, ":");
	char *port = strtok(NULL, "");

	if(!host_name){
		sprintf(debug_text,"tcpclient_thread_function: invalid hostname:%s",host_name);
		debug(debug_text,255);
		return NULL;
	}
	if(!port){
		sprintf(debug_text,"tcpclient_thread_function: invalid port:%s",port);
		debug(debug_text,255);
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

  tcpclient_slot_tcp_sock_array[tcpclient_parms.tcpclient_slot] = tcp_sock;


  if (connect(tcp_sock,(struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
		sprintf(debug_text,"tcpclient_thread_function: failed to connect to %s", host_name);
	  debug(debug_text,255);
		close(tcp_sock);

    tcpclient_slot_tcp_sock_array[tcpclient_parms.tcpclient_slot] = -1;
		return NULL;
   }

	int e;
	while((e = recv(tcp_sock, buff, sizeof(buff), 0)) >= 0){
		if (e > 0){
			buff[e] = 0;
			
			//here we will try stripping too many spaces
			char *p, *q, buff2[201]; // bigger than the original buff
			int n_spaces = 0;
			int tab_space = 3;	
			p = buff; q = buff2;
			for (p = buff;*p;p++){
				if (*p != ' '){ 
					//add spaces for a shorter tab
					if (n_spaces > tab_space){
						int l = strlen(buff2);
						int next_tab = (l/tab_space + 1)* tab_space;
						for (int i = 0; i < next_tab - l; i++){
							*q++ = ' ';
							*q = 0;
						}
					}
					*q++ = *p;
					*q = 0;
					n_spaces = 0;
				}
				else { // *p is a space
					if (n_spaces < tab_space){
						*q++ = *p;
						*q = 0;
					}
					n_spaces++;
				}
			}
			*q = 0;	

//			printf("compressed [%s] to [%s]\n", buff, buff2);

		  sprintf(debug_text,"tcpclient_thread_function: tcpclient_slot:%d received:%s", tcpclient_parms.tcpclient_slot,buff);
	    debug(debug_text,3);

			printf(buff);

		}
	}
	close(tcp_sock);
  tcpclient_slot_tcp_sock_array[tcpclient_parms.tcpclient_slot] = -1;	

}

// ---------------------------------------------------------------------------------------


int tcpclient_write(int tcpclient_slot, char *text){


  int tcp_sock = 0;

	if ((tcpclient_slot < 0) || (tcpclient_slot >= MAX_TCPCLIENTS)) {
		sprintf(debug_text,"tcpclient_write: invalid tcpclient_slot:%d", tcpclient_slot);
	  debug(debug_text,255);		
		return -1;
	}

  tcp_sock = tcpclient_slot_tcp_sock_array[tcpclient_slot];

	if (tcp_sock < 0){
		sprintf(debug_text,"tcpclient_write: tcpclient_slot:%d connection is closed", tcpclient_slot);
	  debug(debug_text,255);		
		return -1;
	}

	sprintf(debug_text,"tcpclient_write: tcpclient_slot:%d sending:%s", tcpclient_slot, text);
  debug(debug_text,1);	
	send (tcp_sock, text, strlen(text), 0);

}

// ---------------------------------------------------------------------------------------


int tcpclient_close(int tcpclient_slot){

  int tcp_sock = 0;

	sprintf(debug_text,"tcpclient_close: closing tcpclient_slot:%d",tcpclient_slot);
	debug(debug_text,2);


	if ((tcpclient_slot < 0) || (tcpclient_slot >= MAX_TCPCLIENTS)) {
		sprintf(debug_text,"tcpclient_close: invalid tcpclient_slot:%d", tcpclient_slot);
	  debug(debug_text,255);		
		return -1;
	}

  tcp_sock = tcpclient_slot_tcp_sock_array[tcpclient_slot];

	if (tcp_sock < 0){
		sprintf(debug_text,"tcpclient_close: invalid tcpclient_slot:%d connection already closed", tcpclient_slot);
	  debug(debug_text,255);		
		return -1;
	}

	close(tcp_sock);
	tcpclient_slot_tcp_sock_array[tcpclient_slot] = -1;

}



// ---------------------------------------------------------------------------------------


int tcpclient_open(int tcpclient_slot, char *server){


	//static char tcp_server_name[100];
	
	//strcpy(tcp_server_name, server);

  struct tcpclient_parms_struct *tcpclient_parms;

  tcpclient_parms = malloc(sizeof(tcpclient_parms));
  tcpclient_parms->server = server;
  tcpclient_parms->tcpclient_slot = tcpclient_slot;


	if (tcpclient_slot < (MAX_TCPCLIENTS-1)){
		sprintf(debug_text,"tcpclient_open: launching thread for:%s tcpclient_slot:%d",tcpclient_parms->server,tcpclient_parms->tcpclient_slot);
		debug(debug_text,2);
	} else {
		sprintf(debug_text,"tcpclient_open: invalid tcpclient_slot");
		debug(debug_text,255);
		return -1;		
	}

  tcpclient_slot_tcp_sock_array[tcpclient_slot] = -2; // -2 means connecting

  pthread_t tcpclient_thread;

 	//pthread_create( &tcpclient_thread, NULL, tcpclient_thread_function, (void*)tcp_server_name);
 	pthread_create( &tcpclient_thread, NULL, tcpclient_thread_function, (void*)tcpclient_parms);

}


// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------


#if !defined(COMPILING_EVERYTHING)

	int main(int argc, char *argv[]){
    
    debug_level = 255;

	  tcpclient_open(2,argv[1]);
	  while(tcpclient_slot_tcp_sock_array[2]<1){
		  sleep(1);	
	  }
	  tcpclient_open(8,argv[1]);
	  while(tcpclient_slot_tcp_sock_array[8]<1){
		  sleep(1);	
	  }


	  int x = 0;
	  while(x++ < 10){
		  tcpclient_write(2,"hello\r");
		  tcpclient_write(8,"hello\r");
		  sleep(1);
	  }
		tcpclient_close(2);
		tcpclient_close(8);
		sleep(3);
	}

#endif
