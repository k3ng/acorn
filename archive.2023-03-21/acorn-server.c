/*

  Acorn Radio - K3NG

  Based on works of Ashhar Farhan, VU2ESE and others



  This is the core / backend of the system.  It provides a TCP listener on a port and takes commands from a GUI, human, etc.


  command line parameters:

  acorn-server

    -d [#] : debug level 1-254; higher = more debugging messages


*/
#include "debug.h"

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
#include "acorn.h"
#include "acorn-server.h"

#include "k3ng.h"
#include "ini.h"
#include "avr.h"
#include "sdr.h"
#include "sound.h"
#include "tcpserver.h"

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

// #include "hamlib.h"
// #include "remote.h"
// #include "wsjtx.h"

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
     // TODO: settings to save
  	{"vfo_a_freq", setting_change_handler_vfo, "", "", TYPE_INTEGER, 0, 30000000, 0, 0},
  	{"vfo_b_freq", setting_change_handler_vfo, "", "", TYPE_INTEGER, 0, 30000000, 0, 0},
  	{"callsign", NULL, "", "", TYPE_INTEGER, 0, 30000000, 0, 0},

    {"", NULL, "", "", TYPE_NULL, 0, 0, 0, 0}

  };


pthread_t control_tcpserver_thread;

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
  signal(SIGINT, signal_handler);

  debug("signal_handler: caught signal, shutting down",DEBUG_LEVEL_BASIC_INFORMATIVE);

  shutdown_flag = 1;

}

// ---------------------------------------------------------------------------------------


int setting_change_handler_vfo(struct setting_struct *passed_setting, char *value){


  return 255;

}


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


  static int fd;
  static struct flock fl;

  if (action == OPEN_LOCK){

    // attempt to create a lock file
    fd = open(LOCK_FILE, O_CREAT|O_RDWR, S_IRWXU);

    if (errno == 13){
    	// lock file already exists
      fd = open(LOCK_FILE, O_RDWR);
    }

    if (fd == -1){
      // we got some sort of problem, just return 1
      fprintf(stderr,"process_lock: Issue with lock file: Error:%d\r\n",errno);
      return RETURN_ERROR;
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
        return RETURN_ERROR;
      }
    }

    return RETURN_NO_ERROR;

  } else if (action == CLOSE_LOCK){
    if (fd){
      close(fd);
      fd = 0;
    }
  } else {
    return RETURN_ERROR;
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

    int x = 0;

    while (argc--){
      if (!strcmp(argv[x], "-d")){
        debug_level = atoi(argv[x+1]);
        sprintf(debug_text,"read_command_line_arguments: debug_level:%d", debug_level);
        debug(debug_text,DEBUG_LEVEL_MANDATORY_NON_ERROR);
      }
      x++;
    }

  #if defined(HARDCODE_DEBUG_LEVEL)

    debug_level = HARDCODE_DEBUG_LEVEL;
    sprintf(debug_text,"read_command_line_arguments: debug_level hardcoded to:%d", debug_level);
    debug(debug_text,DEBUG_LEVEL_MANDATORY_NON_ERROR);     
         
  #endif

}

// ---------------------------------------------------------------------------------------

// TODO: these are dropped in to satisfy modems.c for the moment


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


  if (geteuid() != 0) {
    fprintf(stderr, "App needs to be run as root.  Hint: use sudo...\n\r");
    exit(RETURN_ERROR);
  }

  read_command_line_arguments(argc, argv);

  puts(VERSION_STRING);

  #if defined(SUPRESS_LOOPBACK_PCM_ERRORS)
    supress_loopback_pcm_errors = 1;
    debug("start_things_up: supressing loopback PCM error printing",DEBUG_LEVEL_BASIC_INFORMATIVE);
  #endif  

  if (RETURN_ERROR == process_lock(OPEN_LOCK)){
    debug("start_things_up: there is another Acorn running.  Exiting.",DEBUG_LEVEL_STDERR);
    exit(RETURN_ERROR);
  }

  signal(SIGINT, signal_handler);

}

// ---------------------------------------------------------------------------------------


void wind_things_down(){

  debug("wind_things_down: called",DEBUG_LEVEL_BASIC_INFORMATIVE);
  process_lock(CLOSE_LOCK);

}


// ---------------------------------------------------------------------------------------


void launch_tcp_server_threads(){

  struct tcpserver_parms_struct *tcpserver_parms;

  tcpserver_parms = malloc(sizeof(tcpserver_parms));
  tcpserver_parms->tcpport = TCP_SERVER_PORT_RIG_COMMAND;
  tcpserver_parms->command_handler = &sdr_request;

  if (pthread_create(&control_tcpserver_thread, NULL, tcpserver_main_thread, (void*) tcpserver_parms)){
    sprintf(debug_text,"launch_tcp_server_threads: could not create control_tcpserver_thread port:%d",tcpserver_parms->tcpport);
    debug(debug_text,DEBUG_LEVEL_STDERR);
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

  initialize_avr_bus();

  launch_tcp_server_threads();

  setup_sdr();

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
  
  return RETURN_NO_ERROR;

}
