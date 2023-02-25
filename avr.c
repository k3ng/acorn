/*


  AVR Stuff

  Anthony Good, K3NG

  We call it the AVR Bus as AVR chips can be daisy-chained like so:

                      +-------------+       +-------------+       +-------------+
            TX]------>|RX  AVR 1  TX|------>|RX  AVR 2  TX|------>|RX  AVR 3  TX|-----+
   Pi serial          +-------------+       +-------------+       +-------------+     |
            RX]<----------------------------------------------------------------------+


  If the first AVR receives a command it doesn't recognize, it simply sends it out its TX port
  for the next AVR to evaluate and act upon if it recognizes the command.  Note that this
  makes it required that commands are not duplicated between AVR units on the bus.

  The poll (p) command has special behavior as each AVR on the bus will respond to it.  An AVR 
  will append its identification and software version in the strong out its TX port to the next
  AVR to do the same.  So, for example, three AVRs like above would do something like this:

    Pi TX: p\r\n
    AVR 1 TX: pCW_Si5351_unit:202301161347$\r\n
    AVR 2 TX: pCW_Si5351_unit:202301161347$auto_tuner_unit:202303261500$\r\n
    AVR 3 TX: pCW_Si5351_unit:202301161347$auto_tuner_unit:202303261500$LCD_manager:202411091000$\r\n

  This allows the Pi to poll and discover whatever AVR units are on the bus.



  Standalone compile with:

    gcc -D COMPILING_AVR -g -o avr serial.c debug.c avr.c -pthread



*/


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h> 
#include <string.h>
#include <termios.h>
#include <pthread.h>

#include "acorn.h"
#include "acorn-server.h"
#include "avr.h"
#include "debug.h"
#include "serial.h"

#define DISCOVERED_AVR_UNITS_ARRAY_MAX 5

struct discovered_avr_bus_units_struct {
  int active;
  char name[16];
  char software_version[15];

} discovered_avr_bus_units[DISCOVERED_AVR_UNITS_ARRAY_MAX];

struct command_responses_struct {
  int response_available;
  char response[32];
} command_responses[NUMBER_OF_AVR_COMMAND_DEFINES];

struct avr_incoming_serial_processor_thread_parms_struct{

  char serial_port[16];

};

int avr_bus_online = 0;


// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------

void *avr_incoming_serial_processor(void *passed_serial_processor_parms){

  struct avr_incoming_serial_processor_thread_parms_struct serial_processor_parms 
    = *(struct avr_incoming_serial_processor_thread_parms_struct*)passed_serial_processor_parms;

  char incoming_line[SERIAL_PORT_INCOMING_BUFFER_SIZE+1];
  char tempchar[SERIAL_PORT_INCOMING_BUFFER_SIZE+1];

  avr_bus_online = 1;

  while(!shutdown_flag){

    if (get_from_serial_incoming_buffer_one_line(serial_processor_parms.serial_port,incoming_line)){


      sprintf(debug_text,"avr_incoming_serial_processor: received:%s",incoming_line);
      debug(debug_text,3);

      // trim leading \r \n - not sure why we're getting them
      if ((incoming_line[0] == '\r') || (incoming_line[0] == '\n')){
        strcpy(tempchar,incoming_line+1);
        strcpy(incoming_line,tempchar);
      }
    
      switch(incoming_line[0]){
        case 'a':
          strcpy(command_responses[AVR_COMMAND_SET_FREQ_DDS0].response,incoming_line+1);
          command_responses[AVR_COMMAND_SET_FREQ_DDS0].response_available = 1;
          break;
        case 'b':
          strcpy(command_responses[AVR_COMMAND_SET_FREQ_DDS1].response,incoming_line+1);
          command_responses[AVR_COMMAND_SET_FREQ_DDS0].response_available = 1;
          break;
        case 'c':
          strcpy(command_responses[AVR_COMMAND_SET_FREQ_DDS2].response,incoming_line+1);
          command_responses[AVR_COMMAND_SET_FREQ_DDS0].response_available = 1;
          break;


        case 'd':
          strcpy(command_responses[AVR_COMMAND_QUERY_DDS_STATUS].response,incoming_line+1);
          command_responses[AVR_COMMAND_QUERY_DDS_STATUS].response_available = 1;
          break;

        case 'f':
          strcpy(command_responses[AVR_COMMAND_QUERY_FWD_POWER].response,incoming_line+1);
          command_responses[AVR_COMMAND_QUERY_FWD_POWER].response_available = 1;
          break;
        case 'r':
          strcpy(command_responses[AVR_COMMAND_QUERY_REF_POWER].response,incoming_line+1);
          command_responses[AVR_COMMAND_QUERY_REF_POWER].response_available = 1;
          break;



        default:      
          strcpy(command_responses[AVR_COMMAND_RAW].response,incoming_line+1);
          command_responses[AVR_COMMAND_SET_FREQ_DDS0].response_available = 1;
          break;
      }


    }
    usleep(1000);
  }

  avr_bus_online = 0;
  return RETURN_NO_ERROR;

}

// ---------------------------------------------------------------------------------------


int initialize_avr_bus(){

  char incoming_line[SERIAL_PORT_INCOMING_BUFFER_SIZE+1];
  char tempchar1[SERIAL_PORT_INCOMING_BUFFER_SIZE+1];
  char tempchar2[SERIAL_PORT_INCOMING_BUFFER_SIZE+1];


  for (int x = 0;x < DISCOVERED_AVR_UNITS_ARRAY_MAX;x++){
    discovered_avr_bus_units[x].active = 0;
    strcpy(discovered_avr_bus_units[x].name,"");
    strcpy(discovered_avr_bus_units[x].software_version,""); 
  }

  for (int x = 0;x < NUMBER_OF_AVR_COMMAND_DEFINES;x++){
    command_responses[x].response_available = 0;
    strcpy(command_responses[x].response,"");
  }


  int fd = setup_serial_port(AVR_BUS_SERIAL_PORT, AVR_BUS_SERIAL_BAUD, 0, 1);

  if (fd < 1){
    sprintf(debug_text,"initialize_avr_bus: error attempting to open serial port:%s",AVR_BUS_SERIAL_PORT);
    debug(debug_text,DEBUG_LEVEL_STDERR);
    return RETURN_ERROR;
  }

  sprintf(debug_text,"initialize_avr_bus: opened serial port:%s fd:%d",AVR_BUS_SERIAL_PORT,fd);
  debug(debug_text,2);
  

  /* poll the AVR bus

     example format: ptest_unit_1:202301161301$test_unit_2:202402161302$test_unit_3:202503161303$

  */

  debug("initialize_avr_bus: polling the AVR bus",2);
  send_out_serial_port(AVR_BUS_SERIAL_PORT,"p\r");

  int loops2 = 10;  // number of times to try the p command
  int loops1 = 500; // 500 * 10000uS = 5 seconds waiting for a serial port response
  char *dollarfindchar;
  char *colonfindchar;
  int got_a_line = 0;
  int parsing_error = 0;
  int number_of_discovered_avr_bus_units = 0;

  while (loops2-- > 0){
    parsing_error = 0;
    // wait 5 seconds for a response
    while ((!got_a_line) && (loops1-- > 0)) {
      usleep(10000);
      got_a_line = get_from_serial_incoming_buffer_one_line(AVR_BUS_SERIAL_PORT,incoming_line);
    }

    sprintf(debug_text,"initialize_avr_bus: received:%s",incoming_line);
    debug(debug_text,2);
    
    if (incoming_line[0] == 'p'){
      sprintf(tempchar1,incoming_line+1); // get rid of "p"
      dollarfindchar = strchr(tempchar1, '$');
      while (dollarfindchar){
        // find the first AVR unit
        *dollarfindchar = 0; // make the $ a 0x00 terminator
        sprintf(tempchar2,tempchar1);// put the found AVR unit in tempchar2
        // printf(tempchar2); 
        // printf("\r\n");
        colonfindchar = strchr(tempchar2, ':');
        if (!colonfindchar){
          parsing_error = 1;
        } else {
          //parse out the unit name and software version
          *colonfindchar = 0; // make the : a 0x00 terinator
          strcpy(discovered_avr_bus_units[number_of_discovered_avr_bus_units].name,tempchar2);
          strcpy(discovered_avr_bus_units[number_of_discovered_avr_bus_units].software_version,colonfindchar+1); 
          sprintf(debug_text,"initialize_avr_bus: discovered unit:%s version:%s",discovered_avr_bus_units[number_of_discovered_avr_bus_units].name,
            discovered_avr_bus_units[number_of_discovered_avr_bus_units].software_version);
          debug(debug_text,3);
          discovered_avr_bus_units[number_of_discovered_avr_bus_units].active = 1;
          number_of_discovered_avr_bus_units++;
        }
        // chop off the first AVR unit in the string and get ready to find the next
        sprintf(tempchar2,dollarfindchar+1);
        sprintf(tempchar1,tempchar2);
        dollarfindchar = strchr(tempchar1, '$');
      }
      if ((!parsing_error) && (number_of_discovered_avr_bus_units > 0)){
        loops2 = 0;
      }
    } //if (incoming_line[0] == 'p')
  }  // while (loops2-- > 0){


/// zzzzzzzzzzzzzz

  pthread_t avr_incoming_serial_processor_thread;

  struct avr_incoming_serial_processor_thread_parms_struct *avr_incoming_serial_processor_thread_parms = malloc(sizeof(avr_incoming_serial_processor_thread_parms));
  // avr_incoming_serial_processor_thread_parms = malloc(sizeof(avr_incoming_serial_processor_thread_parms));
  strcpy(avr_incoming_serial_processor_thread_parms->serial_port,AVR_BUS_SERIAL_PORT);


  if ((parsing_error) || (number_of_discovered_avr_bus_units < 1)){
    return RETURN_ERROR;
  } else {
    // launch thread to service incoming responses


   if (pthread_create(&avr_incoming_serial_processor_thread, NULL, avr_incoming_serial_processor, (void*) avr_incoming_serial_processor_thread_parms)){
      debug("initialize_avr_bus: could not create avr_incoming_serial_processing_thread",DEBUG_LEVEL_STDERR);
      return RETURN_ERROR;
    }

  }

  
  return RETURN_NO_ERROR;


}



// ---------------------------------------------------------------------------------------


int send_avr_bus_command(int command,char *arguments){


  char tempchar[32];
  int return_code = RETURN_ERROR;

  if (!avr_bus_online){
    sprintf(debug_text,"send_avr_bus_command: avr bus not online");
    debug(debug_text,DEBUG_LEVEL_STDERR);
    return RETURN_ERROR;
  }

  sprintf(debug_text,"send_avr_bus_command: command:%d args:%s",command,arguments);
  debug(debug_text,DEBUG_LEVEL_SOMEWHST_NOISY_INFORMATIVE);

  strcpy(tempchar,"");

  switch (command){
    case AVR_COMMAND_RAW:
      sprintf(tempchar,"%s\r",arguments);
      break;
    case AVR_COMMAND_SET_FREQ_DDS0:
      sprintf(tempchar,"a%s\r",arguments);
      break;
    case AVR_COMMAND_SET_FREQ_DDS1:
      sprintf(tempchar,"b%s\r",arguments);
      break;
    case AVR_COMMAND_SET_FREQ_DDS2:
      sprintf(tempchar,"c%s\r",arguments);
      break;    
    case AVR_COMMAND_QUERY_DDS_STATUS:
      sprintf(tempchar,"d\r");
      break; 
    case AVR_COMMAND_QUERY_FWD_POWER:
      sprintf(tempchar,"f\r");
      break; 
    case AVR_COMMAND_QUERY_REF_POWER:
      sprintf(tempchar,"r\r");
      break;    

  } // switch (command)

  return_code = send_out_serial_port(AVR_BUS_SERIAL_PORT,tempchar);

  if (return_code == RETURN_ERROR){
    debug("send_avr_bus_command: returning error",DEBUG_LEVEL_STDERR);
  }

  return return_code;

}
// ---------------------------------------------------------------------------------------

int wait_for_avr_bus_command_response_available(int command){


  sprintf(debug_text,"wait_for_avr_bus_command_response_available: command:%d",command);
  debug(debug_text,2);

  while (!command_responses[command].response_available){
    usleep(1000);
  }

  return 1;

}

// ---------------------------------------------------------------------------------------

int avr_bus_command_response_available(int command){

  return command_responses[command].response_available;

}


// ---------------------------------------------------------------------------------------


int get_avr_bus_command_response_int(int command){

  int return_value;

  if(command < NUMBER_OF_AVR_COMMAND_DEFINES){
    if(command_responses[command].response_available){
      command_responses[command].response_available = 0;
      return_value = atoi(command_responses[command].response);
    } else {
      return_value = RETURN_NIL;
    }
  } else {
    return RETURN_ERROR;
  }

  return return_value;

}

// ---------------------------------------------------------------------------------------


char *get_avr_bus_command_response_string(int command){


  static char return_char[32];

  strcpy(return_char,"");

  if(command < NUMBER_OF_AVR_COMMAND_DEFINES){
    if(command_responses[command].response_available){
      command_responses[command].response_available = 0;
      strcpy(return_char,command_responses[command].response);
    } 
  }

  return return_char;


}


// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------

#if !defined(COMPILING_EVERYTHING)


  void main(){

    debug_level = 255;

    initialize_avr_bus();

    usleep(10000);

    int loop = 1;
    int freq = 1000000;
    char tempchar[20];




    strcpy(tempchar,"");
    send_avr_bus_command(AVR_COMMAND_QUERY_DDS_STATUS,tempchar); 
    wait_for_avr_bus_command_response_available(AVR_COMMAND_QUERY_DDS_STATUS);
    strcpy(tempchar,get_avr_bus_command_response_string(AVR_COMMAND_QUERY_DDS_STATUS));
    printf("main: DDS status:%s\r\n",tempchar);

    strcpy(tempchar,"");
    send_avr_bus_command(AVR_COMMAND_QUERY_FWD_POWER,tempchar); 
    wait_for_avr_bus_command_response_available(AVR_COMMAND_QUERY_FWD_POWER);
    strcpy(tempchar,get_avr_bus_command_response_string(AVR_COMMAND_QUERY_FWD_POWER));
    printf("main: forward power reading:%s\r\n",tempchar);

    strcpy(tempchar,"");
    send_avr_bus_command(AVR_COMMAND_QUERY_REF_POWER,tempchar); 
    wait_for_avr_bus_command_response_available(AVR_COMMAND_QUERY_REF_POWER);
    strcpy(tempchar,get_avr_bus_command_response_string(AVR_COMMAND_QUERY_REF_POWER));
    printf("main: reflected power reading:%s\r\n",tempchar);    


    while (loop-- > 0){
      freq = 1000000;
      while (freq < 60000000){
        sprintf(tempchar,"%d",freq);
        printf("main: DDS0:%s MHz\r\n",tempchar);
        send_avr_bus_command(AVR_COMMAND_SET_FREQ_DDS0,tempchar); 
        if (avr_bus_command_response_available(AVR_COMMAND_SET_FREQ_DDS0)){
          strcpy(tempchar,get_avr_bus_command_response_string(AVR_COMMAND_SET_FREQ_DDS0));
          printf("main: command response:%s\r\n",tempchar);
        }
        freq = freq + 5000000;
        usleep(1000000);  

      }     
    }





  }

#endif //!defined(COMPILING_EVERYTHING)