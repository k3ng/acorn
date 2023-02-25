/*

  acorn avr main unit

  Anthony Good, K3NG


  serial commands

    a: set clock 0 frequency
    b: set clock 1 frequency
    c: set clock 2 frequency

    d: query dds status

    f: read forward power voltage
    r: read reflected power voltage

    p: poll the serial ring to identify devices

    TODO s: send CW string
    TODO w: change CW WPM
    TODO x: empty CW buffer
    TODO z: read paddle echo buffer
    TODO R: reverse paddle


*/

#define CODE_VERSION "202301161347"
#define UNIT_TYPE "CW_Si5351_unit"

#include <stdio.h>
#include <avr/pgmspace.h>
#include <Wire.h>
#include <Adafruit_I2CDevice.h>
#include <Adafruit_SI5351.h>


 
#define PIN_LED 7
#define PIN_TX 4
#define PIN_PTT 8
#define PIN_PADDLE_LEFT 2
#define PIN_PADDLE_RIGHT 3
#define PIN_SWR_FWD_V A0
#define PIN_SWR_REV_V A1

#define initial_speed_wpm 20             // keyer speed setting
#define initial_sidetone_freq 600        // sidetone frequency setting
#define char_send_buffer_size 50
#define element_send_buffer_size 20
#define default_length_letterspace 3
#define default_length_wordspace 7
#define initial_ptt_lead_time 0          // PTT lead time in mS
#define initial_ptt_tail_time 0         // PTT tail time in mS
#define default_ptt_hang_time_wordspace_units 0.0 
#define default_serial_baud_rate 115200
#define serial_buffer_size 32

enum key_scheduler_type {IDLE, PTT_LEAD_TIME_WAIT, KEY_DOWN, KEY_UP};
enum sending_type {AUTOMATIC_SENDING, MANUAL_SENDING};
enum element_buffer_type {HALF_UNIT_KEY_UP, ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP, THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP,ONE_UNIT_KEYDOWN_3_UNITS_KEY_UP, THREE_UNIT_KEYDOWN_3_UNITS_KEY_UP,
  ONE_UNIT_KEYDOWN_7_UNITS_KEY_UP, THREE_UNIT_KEYDOWN_7_UNITS_KEY_UP, SEVEN_UNITS_KEY_UP, KEY_UP_LETTERSPACE_MINUS_1, KEY_UP_WORDSPACE_MINUS_4, KEY_UP_WORDSPACE};

#define SERIAL_SEND_BUFFER_WPM_CHANGE 200
#define SERIAL_SEND_BUFFER_PTT_ON 201
#define SERIAL_SEND_BUFFER_PTT_OFF 202
#define SERIAL_SEND_BUFFER_TIMED_KEY_DOWN 203
#define SERIAL_SEND_BUFFER_TIMED_WAIT 204
#define SERIAL_SEND_BUFFER_NULL 205
#define SERIAL_SEND_BUFFER_PROSIGN 206
#define SERIAL_SEND_BUFFER_HOLD_SEND 207
#define SERIAL_SEND_BUFFER_HOLD_SEND_RELEASE 208
#define SERIAL_SEND_BUFFER_MEMORY_NUMBER 210

#define SERIAL_SEND_BUFFER_NORMAL 0
#define SERIAL_SEND_BUFFER_TIMED_COMMAND 1
#define SERIAL_SEND_BUFFER_HOLD 2

#define NORMAL 0
#define OMIT_LETTERSPACE 1

Adafruit_SI5351 clockgen = Adafruit_SI5351();

byte key_scheduler_state = IDLE;
unsigned long next_key_scheduler_transition_time = 0;
unsigned int key_scheduler_keyup_ms;
unsigned int key_scheduler_keydown_ms;
unsigned int wpm = initial_speed_wpm;
unsigned long ptt_time;
byte ptt_line_activated = 0;
byte key_state = 0;
byte key_tx = 1;
byte length_letterspace = default_length_letterspace;
byte length_wordspace = default_length_wordspace;
byte manual_ptt_invoke = 0;
byte last_sending_type = MANUAL_SENDING;
unsigned int ptt_tail_time = initial_ptt_tail_time;
unsigned int ptt_lead_time = initial_ptt_lead_time;
float ptt_hang_time_wordspace_units = default_ptt_hang_time_wordspace_units;

byte incoming_serial_byte;
unsigned long serial_baud_rate;
byte serial_backslash_command;

byte pause_sending_buffer = 0;
byte char_send_buffer_array[char_send_buffer_size];
byte char_send_buffer_bytes = 0;
byte char_send_buffer_status = SERIAL_SEND_BUFFER_NORMAL;

byte element_send_buffer_array[element_send_buffer_size];
byte element_send_buffer_bytes = 0;

int dds_status = 0;


//-------------------------------------------------------------------------------------------------------

void initialize_pins(){

  pinMode(PIN_LED, OUTPUT); 
  pinMode(PIN_TX, OUTPUT);
  digitalWrite(PIN_TX,LOW);
  pinMode(PIN_PTT, OUTPUT);
  digitalWrite(PIN_PTT,LOW);
  pinMode(PIN_PADDLE_LEFT, INPUT_PULLUP);
  pinMode(PIN_SWR_FWD_V, INPUT);
  pinMode(PIN_SWR_REV_V, INPUT);

}
//-------------------------------------------------------------------------------------------------------
void initialize_hardware(){


  if (clockgen.begin() == ERROR_NONE){
    clockgen.enableOutputs(true);
    dds_status = 1;
  } else {
    dds_status = 0;
  }


}


//-------------------------------------------------------------------------------------------------------


void initialize_serial(){

  Serial.begin(default_serial_baud_rate); 

}

//-------------------------------------------------------------------------------------------------------


void set_clk_freq(uint8_t clk, uint32_t frequency){
  
  si5351PLL_t pll;

  if (clk == 1){
    pll = SI5351_PLL_B;
  } else {
    pll = SI5351_PLL_A;
  }

  uint32_t pll_div = 650000000l / frequency;

  //round to the next even integer
  if (pll_div * 650000000l != frequency){
    pll_div++;
  }
 
  if (pll_div & 1){
    pll_div++;
  }

  int32_t xtal_freq_calibrated = 25000000;
  int32_t denom = 0x80000;
  int32_t pllfreq = frequency * pll_div;
  int32_t multi = pllfreq / xtal_freq_calibrated;
  int32_t num = ((uint64_t)(pllfreq % xtal_freq_calibrated) * 0x80000)/xtal_freq_calibrated;  


  clockgen.setupPLL(pll, multi, num, denom);
  clockgen.setupRdiv(clk,SI5351_R_DIV_1);
  clockgen.setupMultisynth(clk, pll, pll_div, 0, 1); 



}

//-------------------------------------------------------------------------------------------------------


int uppercase (int charbytein){

  if ((charbytein > 96) && (charbytein < 123)) {
    charbytein = charbytein - 32;
  }
  return charbytein;
}


//-------------------------------------------------------------------------------------------------------


void blink_the_led(){


  static unsigned long int last_transistion_time = 0;
  static int current_led_state = LOW;
  static unsigned long int led_blink_time = 100;

  if (millis() > 1000){
    led_blink_time = 1000;
  }

  if ((millis() - last_transistion_time) >= led_blink_time){
    if (current_led_state == LOW){
      digitalWrite(PIN_LED, HIGH);
      current_led_state = HIGH;
    } else {
      digitalWrite(PIN_LED, LOW);
      current_led_state = LOW;      
    }
    last_transistion_time = millis();
  }

}

//-------------------------------------------------------------------------------------------------------

void add_to_element_send_buffer(byte element_byte){

  if (element_send_buffer_bytes < element_send_buffer_size) {
    element_send_buffer_array[element_send_buffer_bytes] = element_byte;
    element_send_buffer_bytes++;
  } 

}

//-------------------------------------------------------------------------------------------------------
void send_dit(byte sending_type) {
  add_to_element_send_buffer(ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP);
}

//-------------------------------------------------------------------------------------------------------
void send_dah(byte sending_type) {

  add_to_element_send_buffer(THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP);
}

//-------------------------------------------------------------------------------------------------------

void send_dits(int dits){

  for (;dits > 0;dits--) {
    send_dit(AUTOMATIC_SENDING);
  } 

}

//-------------------------------------------------------------------------------------------------------

void send_dahs(int dahs){
  for (;dahs > 0;dahs--) {
    send_dah(AUTOMATIC_SENDING);
  } 
  
}



//-------------------------------------------------------------------------------------------------------

void remove_from_element_send_buffer(){
  if (element_send_buffer_bytes > 0) {
    element_send_buffer_bytes--;
  }
  if (element_send_buffer_bytes > 0) {
    for (int x = 0;x < element_send_buffer_bytes;x++) {
      element_send_buffer_array[x] = element_send_buffer_array[x+1];
    }
  }
}

//-------------------------------------------------------------------------------------------------------
byte keyer_is_idle() {
  
  if ((!char_send_buffer_bytes) && (!element_send_buffer_bytes) && (!ptt_line_activated) && (key_scheduler_state == IDLE)) {
    return 1;
  } else {
    return 0;
  }
  
}

//-------------------------------------------------------------------------------------------------------

void add_to_char_send_buffer(byte incoming_serial_byte) {

    if (char_send_buffer_bytes < char_send_buffer_size) {
      if (incoming_serial_byte != 127) {
        char_send_buffer_bytes++;
        char_send_buffer_array[char_send_buffer_bytes - 1] = incoming_serial_byte;
      } else {  // we got a backspace
        char_send_buffer_bytes--;
      }
    } 

}


//-------------------------------------------------------------------------------------------------------
void send_character_string(char* string_to_send) {
  
  for (int x = 0;x < 32;x++) {
    if (string_to_send[x] != 0) {
      add_to_char_send_buffer(string_to_send[x]);
    } else {
      x = 33;
    }
  }
}
//-------------------------------------------------------------------------------------------------------


void tx(byte state){

  digitalWrite (PIN_TX, state);

} 


//-------------------------------------------------------------------------------------------------------

void ptt(byte state){

  digitalWrite (PIN_PTT, state);
    
}

//-------------------------------------------------------------------------------------------------------


void ptt_key()
{
  if (ptt_line_activated == 0) {   // if PTT is currently deactivated, bring it up and insert PTT lead time delay
    ptt(HIGH);
    ptt_line_activated = 1;      
  }
  ptt_time = millis();
}

//-------------------------------------------------------------------------------------------------------

void ptt_unkey()
{
  if (ptt_line_activated) {
    ptt(LOW);
    ptt_line_activated = 0;      
  }  
}

//-------------------------------------------------------------------------------------------------------

void tx_key (int state)
{
  if ((state) && (key_state == 0)) {
    if (key_tx) {
      ptt_key();      
      tx(HIGH);
    }
    key_state = 1;
  } else {
    if ((state == 0) && (key_state)) {
      if (key_tx) {
        tx(LOW);
      }
      key_state = 0;
    }          
  }
}  


//-------------------------------------------------------------------------------------------------------

void check_ptt_tail()
{ 
  if ((key_state) || (key_scheduler_state == PTT_LEAD_TIME_WAIT)) {
    ptt_time = millis();
  } else {
    if ((ptt_line_activated) && (manual_ptt_invoke == 0) && ((millis() - ptt_time) > ptt_tail_time)){
      ptt_unkey();
    }
  }
}


//-------------------------------------------------------------------------------------------------------

void schedule_keydown_keyup (unsigned int keydown_ms, unsigned int keyup_ms){

  if (keydown_ms) {
    if ((ptt_lead_time) && (!ptt_line_activated)) {
      ptt_key();
      key_scheduler_state = PTT_LEAD_TIME_WAIT;
      next_key_scheduler_transition_time = millis() + ptt_lead_time;
      key_scheduler_keydown_ms = keydown_ms;
      key_scheduler_keyup_ms = keyup_ms;
    } else {
      tx_key(1);
      key_scheduler_state = KEY_DOWN;
      next_key_scheduler_transition_time = millis() + keydown_ms;
      key_scheduler_keyup_ms = keyup_ms;      
    }
  } else {
    tx_key(0);
    key_scheduler_state = KEY_UP;
    next_key_scheduler_transition_time = millis() + keyup_ms;
  }
  
}

//-------------------------------------------------------------------------------------------------------

void service_key_scheduler(){
  
  switch (key_scheduler_state) {
    case PTT_LEAD_TIME_WAIT:
      if (millis() >= next_key_scheduler_transition_time) {
        tx_key(1);
        key_scheduler_state = KEY_DOWN;
        next_key_scheduler_transition_time = (millis() + key_scheduler_keydown_ms);
      }
      break;        
    case KEY_DOWN:
      if (millis() >= next_key_scheduler_transition_time) {
        tx_key(0);
        key_scheduler_state = KEY_UP;
        if (key_scheduler_keyup_ms) {
          next_key_scheduler_transition_time = (millis() + key_scheduler_keyup_ms);
        } else {
          key_scheduler_state = IDLE;
        }
      }
      break;
    case KEY_UP:
      if (millis() >= next_key_scheduler_transition_time) {
        key_scheduler_state = IDLE;
      }
      break;    
  }
}

//-------------------------------------------------------------------------------------------------------

void remove_from_char_send_buffer()
{
  if (char_send_buffer_bytes > 0) {
    char_send_buffer_bytes--;
  }
  if (char_send_buffer_bytes > 0) {
    for (int x = 0;x < char_send_buffer_bytes;x++) {
      char_send_buffer_array[x] = char_send_buffer_array[x+1];
    }
  }
}

//-------------------------------------------------------------------------------------------------------

void send_char(char cw_char, byte omit_letterspace)
{
  #ifdef DEBUG
  Serial.write("\nsend_char: called with cw_char:");
  Serial.print(cw_char);
  if (omit_letterspace) {
    Serial.print (" OMIT_LETTERSPACE");
  }
  Serial.write("\n\r");
  #endif
  
  if ((cw_char == 10) || (cw_char == 13)) { return; }  // don't attempt to send carriage return or line feed
  
  switch (cw_char) {
    case 'A': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
    case 'B': send_dah(AUTOMATIC_SENDING); send_dits(3); break;
    case 'C': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
    case 'D': send_dah(AUTOMATIC_SENDING); send_dits(2); break;
    case 'E': send_dit(AUTOMATIC_SENDING); break;
    case 'F': send_dits(2); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
    case 'G': send_dahs(2); send_dit(AUTOMATIC_SENDING); break;
    case 'H': send_dits(4); break;
    case 'I': send_dits(2); break;
    case 'J': send_dit(AUTOMATIC_SENDING); send_dahs(3); break;
    case 'K': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
    case 'L': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dits(2); break;
    case 'M': send_dahs(2); break;
    case 'N': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
    case 'O': send_dahs(3); break;
    case 'P': send_dit(AUTOMATIC_SENDING); send_dahs(2); send_dit(AUTOMATIC_SENDING); break;
    case 'Q': send_dahs(2); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
    case 'R': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
    case 'S': send_dits(3); break;
    case 'T': send_dah(AUTOMATIC_SENDING); break;
    case 'U': send_dits(2); send_dah(AUTOMATIC_SENDING); break;    
    case 'V': send_dits(3); send_dah(AUTOMATIC_SENDING); break;
    case 'W': send_dit(AUTOMATIC_SENDING); send_dahs(2); break;
    case 'X': send_dah(AUTOMATIC_SENDING); send_dits(2); send_dah(AUTOMATIC_SENDING); break;
    case 'Y': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dahs(2); break;
    case 'Z': send_dahs(2); send_dits(2); break;
        
    case '0': send_dahs(5); break;
    case '1': send_dit(AUTOMATIC_SENDING); send_dahs(4); break;
    case '2': send_dits(2); send_dahs(3); break;
    case '3': send_dits(3); send_dahs(2); break;
    case '4': send_dits(4); send_dah(AUTOMATIC_SENDING); break;
    case '5': send_dits(5); break;
    case '6': send_dah(AUTOMATIC_SENDING); send_dits(4); break;
    case '7': send_dahs(2); send_dits(3); break;
    case '8': send_dahs(3); send_dits(2); break;
    case '9': send_dahs(4); send_dit(AUTOMATIC_SENDING); break;
    
    case '=': send_dah(AUTOMATIC_SENDING); send_dits(3); send_dah(AUTOMATIC_SENDING); break;
    case '/': send_dah(AUTOMATIC_SENDING); send_dits(2); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
    case ' ': add_to_element_send_buffer(KEY_UP_WORDSPACE_MINUS_4); break;
    case '*': send_dah(AUTOMATIC_SENDING); send_dits(3); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;    // using asterisk for BK
    case '.': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
    case ',': send_dahs(2); send_dits(2); send_dahs(2); break;
    case '\'': send_dit(AUTOMATIC_SENDING); send_dahs(4); send_dit(AUTOMATIC_SENDING); break;                   // apostrophe
    case '!': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dahs(2); break;
    case '(': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dahs(2); send_dit(AUTOMATIC_SENDING); break;
    case ')': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dahs(2); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
    case '&': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dits(3); break;
    case ':': send_dahs(3); send_dits(3); break;
    case ';': send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
    case '+': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
    case '-': send_dah(AUTOMATIC_SENDING); send_dits(4); send_dah(AUTOMATIC_SENDING); break;
    case '_': send_dits(2); send_dahs(2); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;
    case '"': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dits(2); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
    case '$': send_dits(3); send_dah(AUTOMATIC_SENDING); send_dits(2); send_dah(AUTOMATIC_SENDING); break;
    case '@': send_dit(AUTOMATIC_SENDING); send_dahs(2); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;
    case '<': send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); break;     // AR
    case '>': send_dits(3); send_dah(AUTOMATIC_SENDING); send_dit(AUTOMATIC_SENDING); send_dah(AUTOMATIC_SENDING); break;               // SK
    case '\n': break;
    case '\r': break;
    case '|': add_to_element_send_buffer(HALF_UNIT_KEY_UP); return; break;  
    default: send_dits(2); send_dahs(2); send_dits(2); break;
  }  
  if (omit_letterspace != OMIT_LETTERSPACE) {
    add_to_element_send_buffer(KEY_UP_LETTERSPACE_MINUS_1); //this is minus one because send_dit and send_dah have a trailing element space
  }

  
}


//-------------------------------------------------------------------------------------------------------

void service_char_send_buffer() {

  if ((char_send_buffer_bytes > 0) && (pause_sending_buffer == 0) && (element_send_buffer_bytes == 0)) {
    send_char(char_send_buffer_array[0],NORMAL);
    remove_from_char_send_buffer();    
  }
  
}


//-------------------------------------------------------------------------------------------------------


void service_element_send_buffer(){
  
  /*
  enum element_buffer_type {HALF_UNIT_KEY_UP, ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP, THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP,ONE_UNIT_KEYDOWN_3_UNITS_KEY_UP, THREE_UNIT_KEYDOWN_3_UNITS_KEY_UP,
  ONE_UNIT_KEYDOWN_7_UNITS_KEY_UP, THREE_UNIT_KEYDOWN_7_UNITS_KEY_UP, SEVEN_UNITS_KEY_UP, KEY_UP_LETTERSPACE_MINUS_1, KEY_UP_WORDSPACE_MINUS_1};
  */
  
  if ((key_scheduler_state == IDLE) && (element_send_buffer_bytes > 0)) {
    switch(element_send_buffer_array[0]) {
 
       case HALF_UNIT_KEY_UP:
         schedule_keydown_keyup(0,0.5*(1200/wpm));
         remove_from_element_send_buffer();
         break;
      
       case ONE_UNIT_KEY_DOWN_1_UNIT_KEY_UP:
         schedule_keydown_keyup(1200/wpm,1200/wpm);
         remove_from_element_send_buffer();
         break;
       
       case THREE_UNITS_KEY_DOWN_1_UNIT_KEY_UP:
         schedule_keydown_keyup(3*(1200/wpm),1200/wpm);
         remove_from_element_send_buffer();
         break;
         
       case KEY_UP_LETTERSPACE_MINUS_1:
         schedule_keydown_keyup(0,(length_letterspace-1)*(1200/wpm));
         remove_from_element_send_buffer();
         break;         
         
       case KEY_UP_WORDSPACE_MINUS_4:
         schedule_keydown_keyup(0,(length_wordspace-4)*(1200/wpm));
         remove_from_element_send_buffer();
         break;  
 
       case KEY_UP_WORDSPACE:
         schedule_keydown_keyup(0,length_wordspace*(1200/wpm));
         remove_from_element_send_buffer();
         break;         
      
    }
  }
  
}


//---------------------------------------------------------------------


char *command_argument(char* command){

  
  static char substring[serial_buffer_size];

  int x = 0;

  while ((command[x] != 0) && (x < (serial_buffer_size-1))){
    substring[x] = command[x+1];
    x++;
  }

  return substring;

}

//---------------------------------------------------------------------

void check_serial(){
  
  

  static char incoming_serial_buffer[serial_buffer_size];
  static int incoming_serial_buffer_bytes = 0;
  char incoming_serial_byte;

  if (Serial.available()){
    incoming_serial_byte = Serial.read();
    if (incoming_serial_buffer_bytes < (serial_buffer_size - 1)){
      incoming_serial_buffer[incoming_serial_buffer_bytes] = incoming_serial_byte;
      incoming_serial_buffer_bytes++;
    }
   

    // process the incoming buffer if it's full or if we have a carriage return
    if ((incoming_serial_buffer_bytes == (serial_buffer_size - 1)) || (incoming_serial_byte == 13)){
      if (incoming_serial_buffer[incoming_serial_buffer_bytes-1] == 13){
        incoming_serial_buffer[incoming_serial_buffer_bytes-1] = 0;  // remove carriage return
      }

      if (incoming_serial_buffer[0] == 'p'){
        Serial.print(incoming_serial_buffer);  // send along any other devices that were before us on the serial ring

        Serial.print(UNIT_TYPE);
        Serial.print(":");
        Serial.print(CODE_VERSION);
        Serial.println("$");

        //Serial.println("test_unit_1:202301161301$test_unit_2:202402161302$test_unit_3:202503161303$");

      } else if (incoming_serial_buffer[0] == 'a'){ 
        Serial.println("aOK");
        set_clk_freq(0,atol(command_argument(incoming_serial_buffer)));
      } else if (incoming_serial_buffer[0] == 'b'){
        set_clk_freq(1,atol(command_argument(incoming_serial_buffer)));
        Serial.println("bOK");
      } else if (incoming_serial_buffer[0] == 'c'){ 
        set_clk_freq(2,atol(command_argument(incoming_serial_buffer)));   
        Serial.println("cOK");
      } else if (incoming_serial_buffer[0] == 'd'){ 
        if (dds_status == 1){   
          Serial.println("dOK"); 
        } else {
          Serial.println("dERROR"); 
        }       
      } else if (!strcmp(incoming_serial_buffer,"f")){
        Serial.print("f");   
        Serial.println(analogRead(PIN_SWR_FWD_V));    
      } else if (!strcmp(incoming_serial_buffer,"r")){
        Serial.print("r");   
        Serial.println(analogRead(PIN_SWR_REV_V));             
      } else {
        // we don't know this command, so pass it on in case there's another AVR on the serial ring
        Serial.println(incoming_serial_buffer);
      }


      // clear the buffer
      incoming_serial_buffer_bytes = 0;
      strcpy(incoming_serial_buffer,"");
    }

  }

}


//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------


void setup() {                
   
  initialize_pins();
  initialize_serial();
  initialize_hardware();


    // clockgen.setupPLL(SI5351_PLL_B, 24, 2, 3);
    // clockgen.setupMultisynth(0, SI5351_PLL_B, 45, 1, 2); 
    // clockgen.setupMultisynth(1, SI5351_PLL_B, 45, 1, 3);
    // clockgen.setupMultisynth(2, SI5351_PLL_B, 45, 1, 4);  
  //   clockgen.enableOutputs(true);  

  // set_clk_freq(0,1000000);
  // set_clk_freq(1,2000000);
  // set_clk_freq(2,3000000);

}


void loop() {

  blink_the_led();
  service_key_scheduler(); 
  check_ptt_tail();
  service_element_send_buffer();
  service_char_send_buffer();
  check_serial();

}
