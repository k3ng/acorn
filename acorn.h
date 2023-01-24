
#if !defined(_acorn_h_)

#define _acorn_h_

#define VERSION_STRING "acorn 0.1"
#define SETTINGS_FILE "/acorn/user_settings.ini"
#define MAX_SETTING_LENGTH 32


#define OPEN_LOCK 1
#define CLOSE_LOCK 2

#define TYPE_NULL 0
#define TYPE_TEXT 1
#define TYPE_INTEGER 2

#define ACTION_UPDATE 0
// #define ACTION_INCREMENT 1  // not implemented yet
// #define ACTION_DECREMENT 2  // not implemented yet

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

/*



    BAND         PIN_PI_BAND4  PIN_PI_BAND3 PIN_PI_BAND2 PIN_PI_BAND1
 1.0 -  5.5 MHz       0             0            0           1
 5.5 - 10.5 MHz       0             0            1           0
10.5 - 18.5 MHz       0             0            1           1
18.5 - 30.0 MHz       0             1            0           0

6m                    1             0            0           1
2m                    1             0            1           0
70 cm                 1             1            0           0


*/

void isr_enc1();
void isr_enc2();

void debug(char *debug_text, int debug_text_level);

#endif //!defined(_acorn_h_)
