
#if !defined(_tcpserver_h_)

#define _tcpserver_h_

#define TCP_SERVER_INCOMING_BUFFER_SIZE 1024
#define COMMAND_HANDLER_RESPONSE_SIZE ((11*2048)+10)  // this is huge due to fft command (9 bytes per MAX_BINS)

void *tcpserver_main_thread();

#endif //!defined(_tcpserver_h_)