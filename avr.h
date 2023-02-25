#if !defined(_avr_h_)

#define _avr_h_

#define AVR_BUS_SERIAL_PORT "/dev/ttyS0"
#define AVR_BUS_SERIAL_BAUD B115200

#define AVR_COMMAND_RAW 0
#define AVR_COMMAND_SET_FREQ_DDS0 1
#define AVR_COMMAND_SET_FREQ_DDS1 2
#define AVR_COMMAND_SET_FREQ_DDS2 3
#define AVR_COMMAND_QUERY_DDS_STATUS 4
#define AVR_COMMAND_QUERY_FWD_POWER 5
#define AVR_COMMAND_QUERY_REF_POWER 6

#define NUMBER_OF_AVR_COMMAND_DEFINES 7

int initialize_avr_bus();
int send_avr_bus_command(int command,char *arguments);
int get_avr_bus_command_response_int(int command);
char *get_avr_bus_command_response_string(int command);

#endif //!defined(_avr_h_)