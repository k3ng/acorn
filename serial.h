#if !defined(_serial_h_)

#define _serial_h_

#define SERIAL_PORT_INCOMING_BUFFER_SIZE 100

int setup_serial_port(char *portname, int speed, int parity, int should_block);
int send_out_serial_port(char *whichone,char *stuff_to_send);
int get_from_serial_incoming_buffer_one_line(char *whichone,char *stuff_to_get);
int get_from_serial_incoming_buffer_everything(char *whichone,char *stuff_to_get);
int bytes_in_incoming_buffer(char *whichone);


#endif //!defined(_serial_h_)