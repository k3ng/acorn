#if !defined(_acorn_gtk_client_h_)

#define _acorn_gtk_client_h_

#define APP_NAME "Acorn"
//#define HARDCODE_DEBUG_LEVEL 8
#define DEFAULT_SERVER_IP_ADDRESS_COLON_PORT "127.0.0.1:8888"

#define FFT_DATA_PULL_FREQUENCY_MS 100
#define FFT_QUERY_TIMEOUT_MS 2000
#define FFT_QUERY_TIMEOUTS_RESET 5
#define FFT_CONNECTION_INIT_TIMEOUT_MS 5000
#define FFT_CONNECTION_RETRY_TIME_MS 3000

#define NO_BUTTON_BORDERS

#define EXCLUDE_ENCODER_CODE
// #define EXCLUDE_SCALING_CODE

#define WINDOW_NAME "Acorn"
#define WINDOW_ICON_FILE "/home/pi/acorn/icon.png"
// #define DO_NOT_FULLSCREEN

// #define FORCE_SCALE_WIDTH_TO 700
// #define FORCE_SCALE_HEIGHT_TO 380

#define FONT_SCALING_FACTOR 0.85

#define LOGBOOK_COMMAND "mousepad %s/acorn/data/logbook.txt"

#define DEFAULT_CALLSIGN "N0BDY"
#define DEFAULT_GRID "AA00aa"

#define INI_FILE "/acorn/data/user_settings.ini"


#define FONT_FIELD_LABEL 0
#define FONT_FIELD_VALUE 1
#define FONT_LARGE_FIELD 2
#define FONT_LARGE_VALUE 3
#define FONT_SMALL 4
#define FONT_LOG 5
#define FONT_LOG_RX 6
#define FONT_LOG_TX 7
#define FONT_SMALL_FIELD_VALUE 8
#define FONT_SPECTRUM_FREQ 9
#define FONT_VFO_LARGE 10
#define FONT_VFO_SMALL 11

#define COLOR_SELECTED_TEXT 0
#define COLOR_TEXT 1
#define COLOR_TEXT_MUTED 2
#define COLOR_SELECTED_BOX 3 
#define COLOR_BACKGROUND 4
#define COLOR_FREQ 5
#define COLOR_LABEL 6
#define SPECTRUM_BACKGROUND 7
#define SPECTRUM_GRID 8
#define SPECTRUM_PLOT 9
#define SPECTRUM_NEEDLE 10
#define COLOR_CONTROL_BOX 11
#define SPECTRUM_BANDWIDTH 12
#define SPECTRUM_PITCH 13
#define SELECTED_LINE 14

#define MAX_FIELD_LENGTH 128

#define FIELD_NUMBER 0
#define FIELD_BUTTON 1
#define FIELD_TOGGLE 2
#define FIELD_SELECTION 3
#define FIELD_TEXT 4
#define FIELD_STATIC 5
#define FIELD_CONSOLE 6

#define MAX_CONSOLE_BUFFER 10000
#define MAX_LINE_LENGTH 128
#define MAX_CONSOLE_LINES 500

// event ids, some of them are mapped from gtk itself
#define FIELD_DRAW 0
#define FIELD_UPDATE 1 
#define FIELD_EDIT 2
#define MIN_KEY_UP 0xFF52
#define MIN_KEY_DOWN	0xFF54
#define MIN_KEY_LEFT 0xFF51
#define MIN_KEY_RIGHT 0xFF53
#define MIN_KEY_ENTER 0xFF0D
#define MIN_KEY_ESC	0xFF1B
#define MIN_KEY_BACKSPACE 0xFF08
#define MIN_KEY_TAB 0xFF09
#define MIN_KEY_CONTROL 0xFFE3
#define MIN_KEY_F1 0xFFBE
#define MIN_KEY_F2 0xFFBF
#define MIN_KEY_F3 0xFFC0
#define MIN_KEY_F4 0xFFC1
#define MIN_KEY_F5 0xFFC2
#define MIN_KEY_F6 0xFFC3
#define MIN_KEY_F7 0xFFC4
#define MIN_KEY_F8 0xFFC5
#define MIN_KEY_F9 0xFFC6
#define MIN_KEY_F9 0xFFC6
#define MIN_KEY_F10 0xFFC7
#define MIN_KEY_F11 0xFFC8
#define MIN_KEY_F12 0xFFC9
#define COMMAND_ESCAPE '\\'

#define MAX_MODES 11 

#define DISABLED 0
#define ENABLED 1

#define ABSOLUTE 0
#define RELATIVE 1

#define MODE_USB 0
#define MODE_LSB 1
#define MODE_CW 2
#define MODE_CWR 3
#define MODE_NBFM 4 
#define MODE_AM 5 
#define MODE_FT8 6  
#define MODE_PSK31 7 
#define MODE_RTTY 8 
#define MODE_DIGITAL 9 
#define MODE_2TONE 10 

#define FT8_AUTO 2
#define FT8_SEMI 1
#define FT8_MANUAL 0

//cw defines
#define CW_DASH (1)
#define CW_DOT (2)
//straight key, iambic, keyboard
#define CW_STRAIGHT 0
#define CW_IAMBIC	1
#define CW_KBD 2

#define MAX_BINS 2048

#define ENC1_A (13)
#define ENC1_B (12)
#define ENC1_SW (14)

#define ENC2_A (0)
#define ENC2_B (2)
#define ENC2_SW (3)

#define SW5 (22)
#define PTT (7)
#define DASH (21)

#define ENC_FAST 1
#define ENC_SLOW 5

#define DS3231_I2C_ADD 0x68

#define SERVER_CONNECTION_UNINITIALIZED 0
#define SERVER_CONNECTION_ESTABLISHING 1
#define SERVER_CONNECTION_ESTABLISHED 2
#define SERVER_CONNECTION_ERROR 3

#define SERVICE 0
#define RETURN_SERVER_LINK_STATE 1
#define SEND_DATA 2

#endif //_acorn_gtk_client_h_

