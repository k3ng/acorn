/*

  Acorn Radio - K3NG

  Based on works of Ashhar Farhan, VU2ESE and others


*/



#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/types.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/limits.h>

// #include <complex.h>
// #include <fftw3.h>
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
// #include <wiringPi.h>
// #include <wiringSerial.h>
// #include <sys/stat.h>

#include "acorn.h"
#include "ini.h"

// #include "sdr.h"
// #include "sound.h"
// #include "sdr_ui.h"
// #include "hamlib.h"
// #include "remote.h"
// #include "remote.h"
// #include "wsjtx.h"
// #include "i2cbb.h"


// ---------------------------------------------------------------------------------------


void debug(char *debug_text, int debug_text_level);



// Variables ------------------------------------------------------------------------------


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



int debug_level = 0;
char debug_text[64];




// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------


int setting_change_handler_vfo(struct setting_struct *passed_setting, char *value){


  return 255;

}

// ---------------------------------------------------------------------------------------


void debug(char *debug_text, int debug_text_level){

  if (debug_text_level <= debug_level){
  	printf(debug_text);
  	printf("\r\n");
  }

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


int sbitx_process_lock(int action){

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
      fprintf(stderr,"sbitx_process_lock: Issue with lock file: Error:%d\r\n",errno);
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

} //sbitx_process_lock()


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
      if (argc){
        debug_level = atoi(argv[x+1]);
        sprintf(debug_text,"read_command_line_arguments: debug_level: %d\r\n", debug_level);
        debug(debug_text,3);
      }
    }
    x++;
  }


}
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------


int main(int argc, char* argv[]) {

  read_command_line_arguments(argc, argv);

	puts(VERSION_STRING);

	if (!sbitx_process_lock(OPEN_LOCK)){
	  fprintf(stderr,"There is another sBitx running.  Exiting.\r\n");
	  exit(1);
	}

  read_ini_file_into_settings();



//change_setting("vfo_a_freq", ACTION_UPDATE, "7040000");



	sbitx_process_lock(CLOSE_LOCK);
  
  return 0;

}
