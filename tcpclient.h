
#if !defined(_tcpclient_h_)

#define _tcpclient_h_

#define MAX_TCPCLIENTS 16

int tcpclient_open(char *server);
int tcpclient_write(int tcpclient_handle, char *text);
int tcpclient_close(int tcpclient_handle);

struct tcpclient_parms_struct{
	char *server;
	int tcpclient_handle;
};

int tcpclient_handle_tcp_sock_array[MAX_TCPCLIENTS];

#endif //!defined(_tcpclient_h_)