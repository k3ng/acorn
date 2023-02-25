#if !defined(_debug_h_)

#define _debug_h_

// TODO: some defines for standard debug levels

int debug_level;
char debug_text[64];

void debug(char *debug_text_in, int debug_text_level);

#endif