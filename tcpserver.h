
#if !defined(_tcpserver_h_)

#define _tcpserver_h_

void *tcp_connection_handler(void *socket_desc);
void *tcpserver_main_thread();

#endif //!defined(_tcpserver_h_)