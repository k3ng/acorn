
#if !defined(_tcpclient_h_)

#define _tcpclient_h_

#define MAX_TCPCLIENTS 16

int tcpclient_open(int tcpclient_slot, char *server);
int tcpclient_write(int tcpclient_slot, char *text);
int tcpclient_close(int tcpclient_slot);

#endif //!defined(_tcpclient_h_)