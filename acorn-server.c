/*

  Acorn Radio - K3NG

  Based on works of Ashhar Farhan, VU2ESE and others



  This is the core / backend of the system.  It provides a TCP listener on a port and takes commands from a GUI, human, etc.


  command line parameters:

  acorn-server

    -d [#] : debug level 1-9; higher = more debugging messages


*/

// #define HARDCODE_DEBUG_LEVEL 8   // use this for troubleshooting to bypass having to do a command line parm

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/limits.h>
#include <complex.h>
#include <fftw3.h>
#include <wiringPi.h>
#include <pthread.h>
#include <signal.h>

// #include <linux/fb.h>
// #include <sys/types.h>
// #include <stdint.h>
// #include <ctype.h>
// #include <sys/mman.h>
// #include <sys/ioctl.h>
// #include <ncurses.h>
// #include <gtk/gtk.h>
// #include <gdk/gdkkeysyms.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <cairo.h>
// #include <wiringSerial.h>
// #include <sys/stat.h>

#include "acorn-server.h"
#include "debug.h"
#include "ini.h"
#include "sdr.h"
#include "sound.h"
#include "tcpserver.h"


// #include "hamlib.h"
// #include "remote.h"
// #include "wsjtx.h"


// ---------------------------------------------------------------------------------------


//void debug(char *debug_text, int debug_text_level);



// Variables ------------------------------------------------------------------------------

int shutdown_flag = 0;

struct setting_struct {
	char	   *name;
	int		   (*change_handler)(struct setting_struct *passed_setting, char *value);
	char	   label[30];
	char	   value[MAX_SETTING_LENGTH];
	int	     value_type;
	long int min;
	long int max;
  int      step;
  int      dirty_flag;
};


int setting_change_handler_vfo(struct setting_struct *passed_setting, char *value);


struct setting_struct setting[] =
  {

  	{"vfo_a_freq", setting_change_handler_vfo, "", "", TYPE_INTEGER, 0, 30000000, 0, 0},
  	{"vfo_b_freq", setting_change_handler_vfo, "", "", TYPE_INTEGER, 0, 30000000, 0, 0},
  	{"callsign", NULL, "", "", TYPE_INTEGER, 0, 30000000, 0, 0},

    {"", NULL, "", "", TYPE_NULL, 0, 0, 0, 0}

  };






pthread_t tcpserver_thread;

static long time_delta = 0;

// ---------------------------------------------------------------------------------------

// used in modems.c

char mycallsign[12];
char mygrid[12];
char current_macro[32];
char contact_callsign[12];
char contact_grid[10];
char sent_rst[10];
char received_rst[10];
char sent_exchange[10];
char received_exchange[10];
int contest_serial = 0;



// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------

void initialize_hardware(){

  // initialize the pins on the Pi

  wiringPiSetup();

  pinMode(PIN_PI_TX, OUTPUT);
  digitalWrite(PIN_PI_TX, LOW);

  pinMode(PIN_PI_BAND1, OUTPUT);
  digitalWrite(PIN_PI_BAND1, LOW);
  pinMode(PIN_PI_BAND2, OUTPUT);
  digitalWrite(PIN_PI_BAND2, LOW);
  pinMode(PIN_PI_BAND3, OUTPUT);
  digitalWrite(PIN_PI_BAND3, LOW);
  pinMode(PIN_PI_BAND4, OUTPUT);
  digitalWrite(PIN_PI_BAND4, LOW);

  pinMode(PIN_PI_PTT, INPUT);
  pullUpDnControl (PIN_PI_PTT, PUD_UP);
  
  pinMode(PIN_PI_ENC1_A, INPUT);
  pullUpDnControl (PIN_PI_ENC1_A, PUD_UP);
  pinMode(PIN_PI_ENC1_B, INPUT);
  pullUpDnControl (PIN_PI_ENC1_B, PUD_UP);
  pinMode(PIN_PI_ENC1_SWITCH, INPUT);
  pullUpDnControl (PIN_PI_ENC1_SWITCH, PUD_UP);

  pinMode(PIN_PI_ENC2_A, INPUT);
  pullUpDnControl (PIN_PI_ENC2_A, PUD_UP);
  pinMode(PIN_PI_ENC2_B, INPUT);
  pullUpDnControl (PIN_PI_ENC2_B, PUD_UP);
  pinMode(PIN_PI_ENC2_SWITCH, INPUT); 
  pullUpDnControl (PIN_PI_ENC2_SWITCH, PUD_UP);

  wiringPiISR(PIN_PI_ENC1_A, INT_EDGE_BOTH, isr_enc1);
  wiringPiISR(PIN_PI_ENC1_B, INT_EDGE_BOTH, isr_enc1);
  wiringPiISR(PIN_PI_ENC2_A, INT_EDGE_BOTH, isr_enc2);
  wiringPiISR(PIN_PI_ENC2_B, INT_EDGE_BOTH, isr_enc2);

}

// ---------------------------------------------------------------------------------------

void isr_enc1(){


}

// ---------------------------------------------------------------------------------------

void isr_enc2(){


}

// ---------------------------------------------------------------------------------------


void signal_handler(int sig){


  signal(sig, SIG_IGN);
  shutdown_flag = 1;
  signal(SIGINT, signal_handler);

  debug("\r\n\r\n\r\n\r\nsignal_handler: caught signal, shutting down",255);

}

// ---------------------------------------------------------------------------------------


int setting_change_handler_vfo(struct setting_struct *passed_setting, char *value){


  return 255;

}

// ---------------------------------------------------------------------------------------


// void debug(char *debug_text_in, int debug_text_level){

//   if (debug_text_level > 254){ // this debug text is to go out STDERR
//     fprintf(stderr, debug_text_in);
//     fprintf(stderr, "\r\n");
//     fflush(stderr);
//   } else {
//     if (debug_text_level <= debug_level){
//     	printf(debug_text_in);
//     	printf("\r\n");
//       fflush(stdout);
//     }
//   }

// }


// ---------------------------------------------------------------------------------------



int change_setting(char *name, int action, char *value){


  /* returns:

       0 = setting name not found
       1 = setting updated
       2 = exceeded min or max
       > 2 = value returned by change_handler

  */


  int return_value = 0;

  for (int x = 0;setting[x].value_type != TYPE_NULL;x++){
  	if (!strcmp(name,setting[x].name)){
      if (setting[x].change_handler){
        return_value = setting[x].change_handler(&setting[x],value);
      } else {
	      switch (action){
	        case ACTION_UPDATE:
	        	if (setting[x].value_type == TYPE_INTEGER){
	        		if ((atoi(value) > setting[x].min) && (atoi(value) < setting[x].max)) {
	              strcpy(setting[x].value,value);
	              return_value = 1;
	        		} else {
	        			return_value = 2;
	        		}
	        	} else {
	        	  strcpy(setting[x].value,value);
	        	  return_value = 1;
	        	}
	        	break;
	      } // switch (action)
	    }

  	}
  }

	sprintf(debug_text,"change_setting: name:%s action:%d value:%s return_value:%d", name, action, value, return_value);
	debug(debug_text,3);

  return(return_value);

}

// ---------------------------------------------------------------------------------------

int change_setting_int(char *name, int value){

  char value_string[MAX_SETTING_LENGTH];
  sprintf(value_string, "%d", value); 

  int return_value = change_setting(name, ACTION_UPDATE, value_string);

  return(return_value);

}



// ---------------------------------------------------------------------------------------


int process_lock(int action){

  // if action is OPEN_LOCK
  //   0 is returned if there is a process already locking  
  //   1 is returned if lock was established


  static int fd;
  static struct flock fl;

  if (action == OPEN_LOCK){

    // attempt to create a lock file
    fd = open("/var/tmp/sbitx.lock", O_CREAT|O_RDWR, S_IRWXU);

    if (errno == 13){
    	// lock file already exists
      fd = open("/var/tmp/sbitx.lock", O_RDWR);
    }

    if (fd == -1){
      // we got some sort of problem, just return 1
      fprintf(stderr,"process_lock: Issue with lock file: Error:%d\r\n",errno);
      return 1;
    }

    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;        
    fl.l_len = 0;        
    fl.l_pid = getpid();

    // attempt to create a file lock
    if (fcntl(fd, F_SETLK, &fl) == -1){
      // it's already locked
      if ((errno == EACCES) || (errno == EAGAIN)){
        return 0;
      }
    }

    return 1;

  } else if (action == CLOSE_LOCK){
    if (fd){
      close(fd);
      fd = 0;
    }
  } else {
    return -1;
  }

} //process_lock()


// ---------------------------------------------------------------------------------------

static int ini_parse_handler(void* user, const char* section, const char* name, const char* value){

		sprintf(debug_text,"ini_parse_handler: [%s] setting %s = %s", section, name, value);
    debug(debug_text,3);

    char new_name[32];
    char new_value[MAX_SETTING_LENGTH];

    strcpy(new_name,name);
    strcpy(new_value,value);

    change_setting(new_name, ACTION_UPDATE, new_value);

    return 1;
}

// ---------------------------------------------------------------------------------------


int read_ini_file_into_settings(){

	char directory[PATH_MAX];
	char *path = getenv("HOME");
	strcpy(directory, path);
	strcat(directory, SETTINGS_FILE);
  if (ini_parse(directory, ini_parse_handler, NULL) < 0){
    fprintf(stderr, "read_ini_file_into_settings: Unable to load user settings file.\n");
  }

}

// ---------------------------------------------------------------------------------------




void read_command_line_arguments(int argc, char* argv[]) {

  #if !defined(HARDCODE_DEBUG_LEVEL)

    int x = 0;

    while (argc--){
      if (!strcmp(argv[x], "-d")){
        if (argc){
          
            debug_level = atoi(argv[x+1]);
            sprintf(debug_text,"read_command_line_arguments: debug_level: %d", debug_level);
            debug(debug_text,3);
        }
      }
      x++;
    }

  #else

    debug_level = HARDCODE_DEBUG_LEVEL;
    sprintf(debug_text,"read_command_line_arguments: debug_level hardcoded to: %d", debug_level);
    debug(debug_text,0);     
         
  #endif


}

// ---------------------------------------------------------------------------------------

// these are dropped in to satisfy modems.c for the moment - Goody K3NG


int get_tx_data_byte(char *c){
  // //take out the first byte and return it to the modem
  // struct field *f = get_field("#text_in");
  // int length = strlen(f->value);

  // if (f->value[0] == '\\' || !length)
  //   return 0;
  // if (length){
  //   *c = f->value[0];
  //   //now shift the buffer down, hopefully, this copies the trailing null too
  //   for (int i = 0; i < length; i++)
  //     f->value[i] = f->value[i+1];
  // }
  // return length;
  // update_field(f);
  // return *c;
}

int get_tx_data_length(){
  // struct field *f = get_field("#text_in");

  // if (strlen(f->value) == 0)
  //   return 0;

  // if (f->value[0] != COMMAND_ESCAPE)
  //   return strlen(get_field("#text_in")->value);
  // else
  //   return 0;
}

int get_cw_delay(){
 //return cw_delay;
}

int get_cw_input_method(){
  //return cw_input_method;
}

int get_pitch(){
  // struct field *f = get_field("#rx_pitch");
  // return atoi(f->value);
}

int get_cw_tx_pitch(){
  //return cw_tx_pitch;
}

int get_data_delay(){
  //return data_delay;
}

int get_wpm(){
  // struct field *f = get_field("#tx_wpm");
  // return atoi(f->value);
}

int is_in_tx(){
 // return in_tx;
}

int key_poll(){
  // int key = 0;
  
  // if (digitalRead(PTT) == LOW)
  //   key |= CW_DASH;
  // if (digitalRead(DASH) == LOW)
  //   key |= CW_DOT;

  // //printf("key %d\n", key);
  // return key;
}

void tx_on(){};
void tx_off(){};

time_t time_system(){
  if (time_delta)
    return  (millis()/1000l) + time_delta;
  else
    return time(NULL);
}

// ---------------------------------------------------------------------------------------

void start_things_up(int argc, char* argv[]){

  read_command_line_arguments(argc, argv);

  puts(VERSION_STRING);

  if (!process_lock(OPEN_LOCK)){
    fprintf(stderr,"There is another Acorn running.  Exiting.\r\n");
    exit(-1);
  }

  signal(SIGINT, signal_handler);

}

// ---------------------------------------------------------------------------------------


void wind_things_down(){

  debug("wind_things_down: called",1);
  process_lock(CLOSE_LOCK);

}


// ---------------------------------------------------------------------------------------


void launch_tcp_server_threads(){

  struct tcpserver_parms_struct *tcpserver_parms;

  tcpserver_parms = malloc(sizeof(tcpserver_parms));
  tcpserver_parms->tcpport = TCP_SERVER_RIG_COMMAND;
  tcpserver_parms->command_handler = &sdr_request;

  if (pthread_create(&tcpserver_thread, NULL, tcpserver_main_thread, (void*) tcpserver_parms)){
    sprintf(debug_text,"launch_tcp_server_threads: could not create tcpserver_thread port:%d",tcpserver_parms->tcpport);
    debug(debug_text,1);
  }

}

// ---------------------------------------------------------------------------------------


void main_loop(){


  while(!shutdown_flag){
    sleep (1);
  }

}

// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------


int main(int argc, char* argv[]) {


  start_things_up(argc, argv);

  read_ini_file_into_settings();

  initialize_hardware();

  launch_tcp_server_threads();

  setup_sdr();

  //change_setting("vfo_a_freq", ACTION_UPDATE, "7040000");

  char dummy_char[100];

  sdr_request("tx=0",dummy_char);
  sdr_request("r1:gain=80",dummy_char);
  sdr_request("r1:volume=80",dummy_char);
  sdr_request("r1:agc=OFF",dummy_char);
  sdr_request("r1:mode=USB",dummy_char);
  sdr_request("r1:low=50",dummy_char);
  sdr_request("r1:high=3000",dummy_char);
  sdr_request("txmode=USB",dummy_char);
  // sdr_request("txmode=CW",dummy_char);
  sdr_request("r1:freq=7000000",dummy_char);

  main_loop();

	wind_things_down();
  
  return 0;

}
