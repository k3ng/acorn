
#if !defined(_acorn_server_h_)

#define _acorn_server_h_

// Codec settings are in sdr.h
// #define CODEC_WM8731
#define CODEC_IQAUDIO_CODEC_ZERO


#define SETTINGS_FILE "/acorn/user_settings.ini"
#define LOCK_FILE "/var/tmp/acorn-server.lock"
//#define HARDCODE_DEBUG_LEVEL 8


#define MAX_SETTING_LENGTH 32


#define OPEN_LOCK 1
#define CLOSE_LOCK 2

#define TYPE_NULL 0
#define TYPE_TEXT 1
#define TYPE_INTEGER 2

#define ACTION_UPDATE 0

#define PIN_PI_TX 27      // physical pin 36 / GPIO16
#define PIN_PI_PTT 0      // physical pin 11 / GPIO17

#define PIN_PI_ENC1_A 3        // physical pin 15 / GPIO22
#define PIN_PI_ENC1_B 4        // physical pin 16 / GPIO23
#define PIN_PI_ENC1_SWITCH 5   // physical pin 18 / GPIO24
#define PIN_PI_ENC2_A 6        // physical pin 22 / GPIO25
#define PIN_PI_ENC2_B 25       // physical pin 37 / GPIO26
#define PIN_PI_ENC2_SWITCH 2   // physical pin 13 / GPIO27

#define PIN_PI_BAND1 21   // physical pin 29 / GPIO5
#define PIN_PI_BAND2 22   // physical pin 31 / GPIO6
#define PIN_PI_BAND3 11   // physical pin 26 / GPIO7
#define PIN_PI_BAND4 10   // physical pin 24 / GPIO8

#define PIN_PI_BAND_HF 26 // physical pin 32 / GPIO12

#define TCP_SERVER_PORT_RIG_COMMAND 8888

#define SUPRESS_LOOPBACK_PCM_ERRORS

/*


HF BANDS (PIN_PI_BAND_HF = HIGH)
PIN_PI_BAND1: 1.0 -  5.5 MHz 
PIN_PI_BAND2: 5.5 - 10.5 MHz 
PIN_PI_BAND3: 10.5 - 18.5 MHz 
PIN_PI_BAND4: 18.5 - 30.0 MHz 

VHF/UHF BANDS (PIN_PI_BAND_HF = LOW)
PIN_PI_BAND1: 6m              
PIN_PI_BAND2: 2m              
PIN_PI_BAND3: 70 cm           


*/

void isr_enc1();
void isr_enc2();


int shutdown_flag;

// struct command_handler_struct{

//   char *request;
//   char *response;

// };

struct tcpserver_parms_struct{

  int tcpport;
  int (*command_handler)(char *request, char *response);

};

struct tcp_connection_handler_parms_struct{

  int client_sock;
  int (*command_handler)(char *request, char *response);

};

#endif //!defined(_acorn_server_h_)
