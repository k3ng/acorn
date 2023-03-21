#if !defined(_debug_h_)

#define _debug_h_

#define DEBUG_TEXT_SIZE 256

int debug_level;

char debug_text[DEBUG_TEXT_SIZE];

void debug(char *debug_text_in, int debug_text_level);

#endif