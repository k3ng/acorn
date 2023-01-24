
/* 


standalone test compilation (TEST_STANDALONE_COMPILE)

compile with:

gcc -g -o vfo vfo.c -lm



*/

//#define TEST_STANDALONE_COMPILE


#include <math.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <linux/types.h>
#include <complex.h>
#include <fftw3.h>
#include <unistd.h>

#if defined(TEST_STANDALONE_COMPILE)
  #define MAX_PHASE_COUNT (16385)

  struct vfo {
    int freq_hz;
    int phase;
    int phase_increment;
  };

  struct vfo v;
#else
  #include "sdr.h"
#endif

//we define one more than needed to cover the boundary of quadrature
static int	phase_table[MAX_PHASE_COUNT];
int sampling_freq = 96000; 

void vfo_init_phase_table(){
	for (int i = 0; i < MAX_PHASE_COUNT; i++){
		double d = (M_PI/2) * ((double)i)/((double)MAX_PHASE_COUNT);
		phase_table[i] = (int)(sin(d) * 1073741824.0);
	}
}

void vfo_start(struct vfo *v, int frequency_hz, int start_phase){
	v->phase_increment = (frequency_hz * 65536) / sampling_freq;
	v->phase = start_phase;
	v->freq_hz = frequency_hz;
}
 
int vfo_read(struct vfo *v){
	int i = 0;
	if (v->phase < 16384)
		i = phase_table[v->phase];
	else if (v->phase < 32768)
		i = phase_table[32767 - v->phase];
	else if (v->phase < 49152)
		i =  -phase_table[v->phase - 32768];
	else
		i = -phase_table[65535- v->phase];  

	//increment the phase and modulo-65536 
	v->phase += v->phase_increment; 
	v->phase &= 0xffff;

	return i;
}

#if defined(TEST_STANDALONE_COMPILE)

  int main(int argc, char* argv[]) {

  	
  	vfo_init_phase_table();

  	vfo_start(&v, 1000, 0);

  	for (int i = 0; i < 100; i++){
  		printf("%d\r\n", vfo_read(&v));
    }

  }

#endif //defined(TEST_STANDALONE_COMPILE)
