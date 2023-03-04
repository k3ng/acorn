
#if !defined(_tcpclient_h_)

#define _tcpclient_h_

#define MAX_TCPCLIENTS 16
#define TCP_CLIENT_INCOMING_BUFFER_SIZE 64

#define TCPCLIENT_UNALLOCATED -1
#define TCPCLIENT_CONNECTING -2

int tcpclient_open(char *server);
int tcpclient_write_text(int tcpclient_handle, char *text);
int tcpclient_write(int tcpclient_handle, char *buffer, int bytes);
int tcpclient_close(int tcpclient_handle);
int tcpclient_connected(int tcpclient_handle);

int tcpclient_incoming_bytes(int tcpclient_handle);
int tcpclient_read(int tcpclient_handle, int bytes, char *buffer);

void tcpclient_clear_incoming_buffer(int tcpclient_handle);

#endif //!defined(_tcpclient_h_)