/* 

This file is from the sBitx by VU2ESE, and others, and is modified to test the WM8731 audio codec.





1. The sound system is kickstarted by calling sound_thread_start() with the 
device id (as a string).

2. The sound system is run in separate thread and it keeps calling sound_process()
WARNING: sound_process() is being called from a different thread. It should
return quickly before the next set of audio data is due.

3. The left channel is used for rx and the right channel is used for tx.
The left channel takes its input (between 0 and 48 KHz( from the rx, 
demodulates it and writes out to the speaker/audio output.

4. The right channel gets audio data from mic, modulates it as a signal between
0 and 48 KHz and sends it out to right channel output.

5. A number of settings for the sound card like gain, etc can be set by calling
sound_mixer(). search for this function to know how to work this.






This follows the tutorial at http://alsamodular.sourceforge.net/alsa_programming_howto.html

	We are using 4 bytes per sample, 
	each frame is consists of two channels of audio, hence 8 bytes 
  We are shooting for 1024x2 = 2048 samples per period. that is 8K
  At two periods in the buffer, the buffer has to be 16K

	To simply the work, we are picking up some settings for the Wolfson codec
	as it connects to a raspberry pi. These values are interdependent
	and they will work out of the box. It takes the guess work out of
	configuring the Raspberry Pi with Wolfson codec.

	MIXER api

	https://alsa.opensrc.org/HowTo_access_a_mixer_control

https://android.googlesource.com/platform/hardware/qcom/audio/+/jb-mr1-dev/alsa_sound/ALSAMixer.cpp

https://github.com/bear24rw/alsa-utils/blob/master/amixer/amixer.c

There are six kinds of controls:
	playback volume 
	playback switch
	playback enumeration
	capture volume
	capture switch
	capture enumeration

examples of using amixer to mute and unmute:
amixer -c 1  set 'Output Mixer Mic Sidetone' unmute
amixer -c 1  set 'Output Mixer Mic Sidetone' mute


examples of using sound_mixer function:
'Mic' 0/1 = mute/unmute the mic
'Line' 0/1= mute/unmute the line in
'Master' 0-100 controls the earphone volume only, line out remains unaffected
'Input Mux' 1/0 take the input either from the Mic or Line In




standalone test compilation

(I couldn't get this to actually do anything on my test WM8731 setup or the sBitx - 2023-01-22 k3ng)

compile with:

gcc -g -o sound debug.c sound.c -lasound -pthread


*/



#include <stdio.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <complex.h>
#include <fftw3.h>
#include <time.h>

#include "acorn.h"
#include "debug.h"
#include "sound.h"
#include "acorn-server.h"
#include "sdr.h"


#define LOOPBACK_CAPTURE "plughw:2,1"
#define LOOPBACK_PLAY "plughw:1,0"


// #if defined(CODEC_WM8731)
//   #define LOOPBACK_CAPTURE "plughw:2,1"
//   #define LOOPBACK_PLAY "plughw:1,0"
// #endif

// #if defined(CODEC_IQAUDIO_CODEC_ZERO)
//   #define LOOPBACK_CAPTURE "plughw:3,1"
//   #define LOOPBACK_PLAY "plughw:2,0"
// #endif

#if !defined(COMPILING_EVERYTHING)
  #define TEST_STANDALONE_COMPILE_THREAD_START "plughw:0,0"
	// #define TEST_STANDALONE_COMPILE_THREAD_START "plughw:CARD=system"
#endif




struct queue{
  int id;
  int head;
  int tail;
  int stall;
	int *data;
	unsigned int underflow;
	unsigned int overflow;
	unsigned int max_q;
};

static struct queue qloop;

int rate = 96000; /* Sample rate */
static snd_pcm_uframes_t buff_size = 8192; /* Periodsize (bytes) */
static int n_periods_per_buffer = 2;       /* Number of periods */
//static int n_periods_per_buffer = 1024;       /* Number of periods */

static snd_pcm_t *pcm_play_handle=0;   	//handle for the pcm device
static snd_pcm_t *pcm_capture_handle=0;   	//handle for the pcm device
static snd_pcm_t *loopback_play_handle=0;   	//handle for the pcm device
static snd_pcm_t *loopback_capture_handle=0;   	//handle for the pcm device

static snd_pcm_stream_t play_stream = SND_PCM_STREAM_PLAYBACK;	//playback stream
static snd_pcm_stream_t capture_stream = SND_PCM_STREAM_CAPTURE;	//playback stream

static char	*pcm_play_name, *pcm_capture_name;
static snd_pcm_hw_params_t *hwparams;
static snd_pcm_sw_params_t *swparams;
static snd_pcm_hw_params_t *hloop_params;
static snd_pcm_sw_params_t *sloop_params;
static int exact_rate;   /* Sample rate returned by */
static int	sound_thread_continue = 0;
pthread_t sound_thread, loopback_thread;

#define LOOPBACK_LEVEL_DIVISOR 8				// Constant used to reduce audio level to the loopback channel (FLDIGI)
static int play_write_error = 0;				// count play channel write errors
static int loopback_write_error = 0;			// count loopback channel write errors

int use_virtual_cable = 0;

int supress_loopback_pcm_errors = 0;

static int count = 0;
static struct timespec gettime_now;
static long int last_time = 0;
static long int last_sec = 0;
static int nframes = 0;
int32_t resample_in[10000];
int32_t resample_out[10000];

int last_second = 0;
int nsamples = 0;
int played_samples = 0;

// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------



void q_empty(struct queue *p){
  
  p->head = 0;
  p->tail = 0;
  p->stall = 1;
	p->underflow = 0;
	p->overflow = 0;
}

// ---------------------------------------------------------------------------------------


void q_init(struct queue *p, int length){

	sprintf(debug_text,"q_init: queue:0x%08x length:%d", p, length);
	debug(debug_text,6);

	p->max_q = length;
	p->data = malloc((length+2) * sizeof(int32_t));
	memset(p->data, 0, p->max_q+1);
	q_empty(p);
}

// ---------------------------------------------------------------------------------------


int q_length(struct queue *p){

  if ( p->head >= p->tail)
    return p->head - p->tail;
  else
    return ((p->head + p->max_q) - p->tail);

}

// ---------------------------------------------------------------------------------------


int q_write(struct queue *p, int32_t w){


  if ( (p->head + 1 == p->tail) || ((p->tail == 0) && (p->head == p->max_q-1)) ) {
    p->overflow++; 
    return RETURN_ERROR;
  }

  p->data[p->head++] = w;

  if (p->head > p->max_q){
    p->head = 0;
  }

	return RETURN_NO_ERROR;
}

// ---------------------------------------------------------------------------------------


int32_t q_read(struct queue *p){

  int32_t data;

  if (p->tail == p->head){
    p->underflow++;
    return (int)0;
  }
    
  data = p->data[p->tail++];
  if (p->tail > p->max_q)
    p->tail = 0;

  return data;
}

// ---------------------------------------------------------------------------------------


void setup_audio_codec(char *audio_card){
  
	//strcpy(audio_card, AUDIO_CARD_NAME);

	sprintf(debug_text,"setup_audio_codec: called audio_card:%s",audio_card);
	debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	//configure all the channels of the mixer

  #if defined(CODEC_WM8731)

  	debug("setup_audio_codec: Input Mux",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
  	sound_mixer(audio_card, "Input Mux",CONTROL_DEFAULT, 0);

  	debug("setup_audio_codec: Line",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
  	sound_mixer(audio_card, "Line",CONTROL_DEFAULT, 1);

    debug("setup_audio_codec: Mic",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
  	sound_mixer(audio_card, "Mic",CONTROL_DEFAULT, 0);

    debug("setup_audio_codec: Mic Boost",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
  	sound_mixer(audio_card, "Mic Boost",CONTROL_DEFAULT, 0);

    debug("setup_audio_codec: Playback Deemphasis",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
  	sound_mixer(audio_card, "Playback Deemphasis",CONTROL_DEFAULT, 0);

    debug("setup_audio_codec: Master",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);   
  	sound_mixer(audio_card, AUDIO_CARD_ELEMENT_OUTPUT,CONTROL_DEFAULT, 10);

    debug("setup_audio_codec: Output Mixer HiFi",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
  	sound_mixer(audio_card, "Output Mixer HiFi",CONTROL_DEFAULT, 1);

    debug("setup_audio_codec: Output Mixer Mic Sidetone",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
  	sound_mixer(audio_card, "Output Mixer Mic Sidetone",CONTROL_DEFAULT, 0);

  #endif //CODEC_WM8731

  #if defined(CODEC_IQAUDIO_CODEC_ZERO)	
 

    sprintf(debug_text,"setup_audio_codec: %s",AUDIO_CARD_ELEMENT_INPUT);
    debug(debug_text,DEBUG_LEVEL_BASIC_INFORMATIVE);
		sound_mixer(audio_card, AUDIO_CARD_ELEMENT_INPUT,CONTROL_CAPTURE_VOLUME_ALL, 0);    


    sprintf(debug_text,"setup_audio_codec: %s",AUDIO_CARD_ELEMENT_OUTPUT);
    debug(debug_text,DEBUG_LEVEL_BASIC_INFORMATIVE);  
		sound_mixer(audio_card, AUDIO_CARD_ELEMENT_OUTPUT,CONTROL_PLAYBACK_VOLUME_ALL, 10);


  #endif //CODEC_IQAUDIO_CODEC_ZERO

}


// ---------------------------------------------------------------------------------------


void sound_mixer(char *card_name, char *element, int control, int set_value){



    long min, max;
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    char *card = card_name;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, card);
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, element);
    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);

    int elem_has_capture_switch;
    int elem_has_playback_switch; 
    int elem_has_playback_volume;
    int elem_has_capture_volume;
    int elem_is_enumerated;

    // #define CONTROL_DEFAULT 0
    // #define CONTROL_CAPTURE_SWITCH_ALL 1
    // #define CONTROL_PLAYBACK_SWITCH_ALL 2
    // #define CONTROL_CAPTURE_VOLUME_ALL 3
    // #define CONTROL_PLAYBACK_VOLUME_ALL 4
    // #define CONTROL_ENUM_ITEM 5


    if (elem){

      elem_has_capture_switch = snd_mixer_selem_has_capture_switch(elem);
      elem_has_playback_switch = snd_mixer_selem_has_playback_switch(elem);
      elem_has_playback_volume = snd_mixer_selem_has_playback_volume(elem);
      elem_has_capture_volume = snd_mixer_selem_has_capture_volume(elem);
      elem_is_enumerated = snd_mixer_selem_is_enumerated(elem);
    

      sprintf(debug_text,"sound_mixer: card_name:%s element:%s control:%d set_value:%d elem:0x%08x capture_sw:%d playback_sw:%d playback_vol:%d capture_vol:%d is_enum:%d"
        ,card_name, element, control, set_value, elem, elem_has_capture_switch, elem_has_playback_switch,
        elem_has_playback_volume, elem_has_capture_volume , elem_is_enumerated);
      debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);


      if(control == CONTROL_DEFAULT){
        if(elem_has_capture_switch){  
          debug("sound_mixer: set capture switch",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);  
          snd_mixer_selem_set_capture_switch_all(elem, set_value);
        } else if (elem_has_playback_switch){
          debug("sound_mixer: set playback switch",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
          snd_mixer_selem_set_playback_switch_all(elem, set_value);
        } else if (elem_has_playback_volume){
          debug("sound_mixer: set playback volume",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
          snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
          snd_mixer_selem_set_playback_volume_all(elem, (long)set_value * max / 100);
        } else if (elem_has_capture_volume){
          debug("sound_mixer: set capture volume",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
          snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
          snd_mixer_selem_set_capture_volume_all(elem, (long)set_value * max / 100);
        } else if (elem_is_enumerated){
          debug("sound_mixer: set enumerated capture element",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
          snd_mixer_selem_set_enum_item(elem, 0, set_value);
        }
      } //CONTROL_DEFAULT


      if (control == CONTROL_CAPTURE_SWITCH_ALL){
  	    if(elem_has_capture_switch){	
  	      debug("sound_mixer: set capture switch",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);  
  		  	snd_mixer_selem_set_capture_switch_all(elem, set_value);
  			} else {
          sprintf(debug_text,"sound_mixer: element %s lacks control %d",element,control);
          debug(debug_text,DEBUG_LEVEL_STDERR);
        }
      }

      if (control == CONTROL_PLAYBACK_SWITCH_ALL){
        if (elem_has_playback_switch){
          debug("sound_mixer: set playback switch",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
    			snd_mixer_selem_set_playback_switch_all(elem, set_value);
    		} else {
          sprintf(debug_text,"sound_mixer: element %s lacks control %d",element,control);
          debug(debug_text,DEBUG_LEVEL_STDERR);
        }
      }  

      if (control == CONTROL_PLAYBACK_VOLUME_ALL){
        if (elem_has_playback_volume){
          debug("sound_mixer: set playback volume",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
        	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
        	snd_mixer_selem_set_playback_volume_all(elem, (long)set_value * max / 100);
        }	else {
          sprintf(debug_text,"sound_mixer: element %s lacks control %d",element,control);
          debug(debug_text,DEBUG_LEVEL_STDERR);
        }
      }  

      if (control == CONTROL_CAPTURE_VOLUME_ALL){
        if (elem_has_capture_volume){
    	    debug("sound_mixer: set capture volume",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
        	snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
        	snd_mixer_selem_set_capture_volume_all(elem, (long)set_value * max / 100);
        } else {
          sprintf(debug_text,"sound_mixer: element %s lacks control %d",element,control);
          debug(debug_text,DEBUG_LEVEL_STDERR);
        }
      }

      if (control == CONTROL_ENUM_ITEM){
    		if (elem_is_enumerated){
    			debug("sound_mixer: set enumerated capture element",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
    			snd_mixer_selem_set_enum_item(elem, 0, set_value);
    		} else {
          sprintf(debug_text,"sound_mixer: element %s lacks control %d",element,control);
          debug(debug_text,DEBUG_LEVEL_STDERR);
        }
      }


    } else { // if (elem)

      sprintf(debug_text,"sound_mixer: element not found: card_name:%s element:%s set_value:%d"
        ,card_name, element);
      debug(debug_text,DEBUG_LEVEL_STDERR);

    } // if (elem)

    snd_mixer_close(handle);
}

// ---------------------------------------------------------------------------------------


int sound_start_play(char *device){

  /* 

  this function should be called just once in the application process.
  Calling it frequently will result in more allocation of hw_params memory blocks
  without releasing them.
  The list of PCM devices available on any platform can be found by running
  	aplay -L 
  We have to pass the id of one of those devices to this function.
  The sequence of the alsa functions must be maintained for this to work consistently



  IMPORTANT:
  The sound is playback is carried on in a non-blocking way  

  */


	snd_pcm_hw_params_alloca(&hwparams);	//more alloc

	//puts a playback handle into the pointer to the pointer
	int e = snd_pcm_open(&pcm_play_handle, device, play_stream, SND_PCM_NONBLOCK);
	
	if (e < 0) {
		sprintf(debug_text,"sound_start_play: error opening PCM playback device %s: %s", device, snd_strerror(e));
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	//fills up the hwparams with values, hwparams was allocated above
	e = snd_pcm_hw_params_any(pcm_play_handle, hwparams);

	if (e < 0) {
		sprintf(debug_text,"sound_start_play: error getting hw playback params (%d)", e);
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	// set the pcm access to interleaved
	e = snd_pcm_hw_params_set_access(pcm_play_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (e < 0) {
		debug("sound_start_play: error setting playback access",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

  /* Set sample format */
	e = snd_pcm_hw_params_set_format(pcm_play_handle, hwparams, SND_PCM_FORMAT_S32_LE);
	if (e < 0) {
		debug("sound_start_play: error setting plyaback format.",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}


	/* Set sample rate. If the exact rate is not supported */
	/* by the hardware, use nearest possible rate.         */ 
	exact_rate = rate;
	e = snd_pcm_hw_params_set_rate_near(pcm_play_handle, hwparams, &exact_rate, 0);
	if (e < 0) {
		debug("sound_start_play: error setting playback rate",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}
	if (rate != exact_rate){
		sprintf(debug_text,"sound_start_play: the playback rate %d changed to %d Hz", rate, exact_rate);
		debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
	}
	else {
		sprintf(debug_text,"sound_start_play: playback sampling rate is set to %d", exact_rate);
		debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
	}


	/* Set number of channels */
	if ((e = snd_pcm_hw_params_set_channels(pcm_play_handle, hwparams, 2)) < 0) {
		debug("sound_start_play: error setting playback channels",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}


	// frame = bytes_per_sample x n_channel
	// period = frames transfered at a time (160 for voip, etc.)
	// we use two periods per buffer.
	if ((e = snd_pcm_hw_params_set_periods(pcm_play_handle, hwparams, n_periods_per_buffer, 0)) < 0) {
		debug("sound_start_play: error setting playback periods",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}


	// the buffer size is each periodsize x n_periods
	snd_pcm_uframes_t  n_frames= (buff_size  * n_periods_per_buffer)/8;
	sprintf(debug_text,"sound_start_play: trying for buffer size of %ld", n_frames);
	debug(debug_text,2);
	e = snd_pcm_hw_params_set_buffer_size_near(pcm_play_handle, hwparams, &n_frames);
	if (e < 0) {
		    debug("sound_start_play: error setting playback buffersize",DEBUG_LEVEL_STDERR);
		    return RETURN_ERROR;
	}

	if (snd_pcm_hw_params(pcm_play_handle, hwparams) < 0) {
		sprintf(debug_text,"sound_start_play: error setting playback HW params");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}
	debug("sound_start_play: all hw params set to play sound",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	return RETURN_NO_ERROR;
}

// ---------------------------------------------------------------------------------------


int sound_start_loopback_capture(char *device){

	debug("sound_start_loopback_capture: called",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	snd_pcm_hw_params_alloca(&hloop_params);
	sprintf (debug_text,"sound_start_loopback_capture: opening audio tx stream to %s", device); 
	debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
	int e = snd_pcm_open(&loopback_capture_handle, device, capture_stream, 0);
	
	if (e < 0) {
		sprintf(debug_text,"sound_start_loopback_capture: error opening loop capture  %s: %s", device, snd_strerror(e));
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	e = snd_pcm_hw_params_any(loopback_capture_handle, hloop_params);

	if (e < 0) {
		sprintf(debug_text,"sound_start_loopback_capture: error setting capture access (%d)", e);
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	e = snd_pcm_hw_params_set_access(loopback_capture_handle, hloop_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (e < 0) {
		debug("sound_start_loopback_capture: error setting capture access",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

  /* Set sample format */
	e = snd_pcm_hw_params_set_format(loopback_capture_handle, hloop_params, SND_PCM_FORMAT_S32_LE);
	if (e < 0) {
		debug("sound_start_loopback_capture: error setting loopback capture format",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	/* Set sample rate. If the exact rate is not supported */
	/* by the hardware, use nearest possible rate.         */ 
	exact_rate = 48000;
	sprintf(debug_text,"sound_start_loopback_capture: setting loopback capture rate to %d", exact_rate);
	debug(debug_text,2);
	e = snd_pcm_hw_params_set_rate_near(loopback_capture_handle, hloop_params, &exact_rate, 0);
	if (e < 0) {
		debug("sound_start_loopback_capture: error setting loopback capture rate",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	if (48000 != exact_rate){
		sprintf(debug_text,"sound_start_loopback_capture: the loopback capture rate set to %d Hz", exact_rate);
	  debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
	}

	/* Set number of channels */
	if ((e = snd_pcm_hw_params_set_channels(loopback_capture_handle, hloop_params, 2)) < 0) {
		debug("sound_start_loopback_capture: error setting loopback capture channels",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	sprintf(debug_text,"sound_start_loopback_capture: %d: set the number of loopback capture channels ", __LINE__);
	debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	/* Set number of periods. Periods used to be called fragments. */ 
	if ((e = snd_pcm_hw_params_set_periods(loopback_capture_handle, hloop_params, n_periods_per_buffer, 0)) < 0) {
		debug("sound_start_loopback_capture: error setting loopback capture periods",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	// the buffer size is each periodsize x n_periods
	snd_pcm_uframes_t  n_frames= (buff_size  * n_periods_per_buffer)/ 8;
	//printf("trying for buffer size of %ld\n", n_frames);
	e = snd_pcm_hw_params_set_buffer_size_near(loopback_capture_handle, hloop_params, &n_frames);
	if (e < 0) {
    debug("sound_start_loopback_capture: error setting loopback capture buffersize",DEBUG_LEVEL_STDERR);
    return RETURN_ERROR;
	}

	sprintf(debug_text,"sound_start_loopback_capture: %d: set buffer to %d", __LINE__, n_frames);
	debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	if (snd_pcm_hw_params(loopback_capture_handle, hloop_params) < 0) {
		debug("sound_start_loopback_capture: error setting capture HW params",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	sprintf(debug_text,"sound_start_loopback_capture: %d: set hwparams", __LINE__);
	debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	/* set some parameters in the driver to handle the latencies */
	snd_pcm_sw_params_malloc(&sloop_params);
	if((e = snd_pcm_sw_params_current(loopback_capture_handle, sloop_params)) < 0){
		sprintf(debug_text,"sound_start_loopback_capture: error getting current loopback capture sw params : %s", snd_strerror(e));
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}
	
	if ((e = snd_pcm_sw_params_set_start_threshold(loopback_capture_handle, sloop_params, 15)) < 0){
		debug("sound_start_loopback_capture: unable to set threshold mode for loopback capture",DEBUG_LEVEL_STDERR);
	} 
	
	if ((e = snd_pcm_sw_params_set_stop_threshold(loopback_capture_handle, sloop_params, 1)) < 0){
		debug("sound_start_loopback_capture: unable to set stop threshold for loopback  capture",DEBUG_LEVEL_STDERR);
	}

	return RETURN_NO_ERROR;
}

// ---------------------------------------------------------------------------------------

int sound_start_capture(char *device){

  /*

  The capture is opened in a blocking mode, the read function will block until 
  there are enough samples to return a block.
  This ensures that the blocks are returned in perfect timing with the codec's clock
  Once you process these captured samples and send them to the playback device, you
  just wait for the next block to arrive 

  */


	snd_pcm_hw_params_alloca(&hwparams);

	int e = snd_pcm_open(&pcm_capture_handle, device,  	capture_stream, 0);

  sprintf(debug_text,"sound_start_capture: device:%s",device);
  debug(debug_text,2);

	
	if (e < 0) {
		sprintf(debug_text,"sound_start_capture: error opening PCM capture device %s: %s", pcm_capture_name, snd_strerror(e));
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	e = snd_pcm_hw_params_any(pcm_capture_handle, hwparams);

	if (e < 0) {
		sprintf(debug_text,"sound_start_capture: error setting capture access (%d)", e);
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	e = snd_pcm_hw_params_set_access(pcm_capture_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (e < 0) {
		debug("sound_start_capture: error setting capture access",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

  /* Set sample format */
	e = snd_pcm_hw_params_set_format(pcm_capture_handle, hwparams, SND_PCM_FORMAT_S32_LE);
	if (e < 0) {
		debug("sound_start_capture, error setting capture format",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}


	/* Set sample rate. If the exact rate is not supported */
	/* by the hardware, use nearest possible rate.         */ 
	exact_rate = rate;
	e = snd_pcm_hw_params_set_rate_near(pcm_capture_handle, hwparams, &exact_rate, 0);
	if ( e< 0) {
		debug("sound_start_capture: error setting capture rate",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	if (rate != exact_rate){
		sprintf(debug_text,"sound_start_capture: the capture rate %d changed to %d Hz", rate, exact_rate);
	  debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
	}


	/* Set number of channels */
	if ((e = snd_pcm_hw_params_set_channels(pcm_capture_handle, hwparams, 2)) < 0) {
		debug("sound_start_capture: error setting capture channels",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}


	/* Set number of periods. Periods used to be called fragments. */ 
	if ((e = snd_pcm_hw_params_set_periods(pcm_capture_handle, hwparams, n_periods_per_buffer, 0)) < 0) {
		debug("sound_start_capture: error setting capture periods",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}


	// the buffer size is each periodsize x n_periods
	snd_pcm_uframes_t  n_frames= (buff_size  * n_periods_per_buffer)/ 8;
	//printf("trying for buffer size of %ld\n", n_frames);
	e = snd_pcm_hw_params_set_buffer_size_near(pcm_play_handle, hwparams, &n_frames);
	if (e < 0) {
    debug("sound_start_capture: error setting capture buffersize",DEBUG_LEVEL_STDERR);
    return RETURN_ERROR;
	}

	if (snd_pcm_hw_params(pcm_capture_handle, hwparams) < 0) {
		debug("sound_start_capture: error setting capture HW params",DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	return RETURN_NO_ERROR;
}

// ---------------------------------------------------------------------------------------


int sound_start_loopback_play(char *device){
	

	snd_pcm_hw_params_alloca(&hwparams);	//more alloc

	sprintf(debug_text,"sound_start_loopback_play: opening audio rx stream to %s", device); 
	debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	int e = snd_pcm_open(&loopback_play_handle, device, play_stream, SND_PCM_NONBLOCK);
	
	if (e < 0) {
		sprintf(debug_text,"sound_start_loopback_play: error opening loopback playback device %s: %s", device, snd_strerror(e));
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	e = snd_pcm_hw_params_any(loopback_play_handle, hwparams);

	if (e < 0) {
		sprintf(debug_text,"sound_start_loopback_play: error getting loopback playback params (%d)", e);
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	e = snd_pcm_hw_params_set_access(loopback_play_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (e < 0) {
		sprintf(debug_text,"sound_start_loopback_play: error setting loopback access");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

  /* Set sample format */
	e = snd_pcm_hw_params_set_format(loopback_play_handle, hwparams, SND_PCM_FORMAT_S32_LE);
	if (e < 0) {
		sprintf(debug_text,"sound_start_loopback_play: error setting loopback format");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	/* Set sample rate. If the exact rate is not supported */
	/* by the hardware, use nearest possible rate.         */ 
	exact_rate = 48000;
	e = snd_pcm_hw_params_set_rate_near(loopback_play_handle, hwparams, &exact_rate, 0);
	if ( e< 0) {
		sprintf(debug_text,"sound_start_loopback_play: error setting playback rate");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}
	if (48000 != exact_rate){
		sprintf(debug_text,"sound_start_loopback_play: the loopback playback rate %d changed to %d Hz", rate, exact_rate);
		debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
	}


	/* Set number of channels */
	if ((e = snd_pcm_hw_params_set_channels(loopback_play_handle, hwparams, 2)) < 0) {
		sprintf(debug_text,"sound_start_loopback_play: error setting playback channels");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}


	// frame = bytes_per_sample x n_channel
	// period = frames transfered at a time (160 for voip, etc.)
	// we use two periods per buffer.
	if ((e = snd_pcm_hw_params_set_periods(loopback_play_handle, hwparams, 8, 0)) < 0) {
		sprintf(debug_text,"sound_start_loopback_play: error setting playback periods");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}


	// the buffer size is each periodsize x n_periods
	snd_pcm_uframes_t  n_frames= (buff_size  * n_periods_per_buffer)/8;
	//lets pump it up to see if we can reduce the dropped frames
	n_frames *= 4;
	sprintf(debug_text,"sound_start_loopback_play: trying for loopback buffer size of %ld", n_frames);
	debug(debug_text,2);
	e = snd_pcm_hw_params_set_buffer_size_near(loopback_play_handle, hwparams, &n_frames);
	if (e < 0) {
    sprintf(debug_text,"sound_start_loopback_play: error setting loopback playback buffersize");
    debug(debug_text,DEBUG_LEVEL_STDERR);
    return RETURN_ERROR;
	}

	sprintf(debug_text,"sound_start_loopback_play: loopback playback buffer size is set to %d", n_frames);
	debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	if (snd_pcm_hw_params(loopback_play_handle, hwparams) < 0) {
		sprintf(debug_text,"sound_start_loopback_play: error setting loopback playback HW params");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return RETURN_ERROR;
	}

	return RETURN_NO_ERROR;
}

// ---------------------------------------------------------------------------------------


// this is only a test process to be substituted to try loopback 
// it was used to debug timing errors
// void sound_process2(int32_t *input_i, int32_t *input_q, int32_t *output_i, int32_t *output_q, int n_samples){
 
// 	for (int i= 0; i < n_samples; i++){
// 		output_i[i] = input_q[i];
// 		output_q[i] = 0;
// 	}	
// }

// ---------------------------------------------------------------------------------------




void sound_stop(){


  //check that we haven't free()-ed up the hwparams block
  //don't call this function at all until that is fixed
  //you don't have to call it anyway

  debug("sound_stop: called",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	snd_pcm_drop(pcm_play_handle);
	snd_pcm_drain(pcm_play_handle);

	snd_pcm_drop(pcm_capture_handle);
	snd_pcm_drain(pcm_capture_handle);
}




// ---------------------------------------------------------------------------------------


int sound_loop(){

  int32_t		*line_in, *line_out, *data_in, *data_out, 
						*input_i, *output_i, *input_q, *output_q;
  int pcmreturn, i, j, loopreturn;
  short s1, s2;
  int frames;

	//we allocate enough for two channels of int32_t sized samples	
  data_in = (int32_t *)malloc(buff_size * 2);
  line_in = (int32_t *)malloc(buff_size * 2);
  line_out = (int32_t *)malloc(buff_size * 2);
  data_out = (int32_t *)malloc(buff_size * 2);
  input_i = (int32_t *)malloc(buff_size * 2);
  output_i = (int32_t *)malloc(buff_size * 2);
  input_q = (int32_t *)malloc(buff_size * 2);
  output_q = (int32_t *)malloc(buff_size * 2);

  frames = buff_size / 8;
	
  snd_pcm_prepare(pcm_play_handle);
  snd_pcm_prepare(loopback_play_handle);
  snd_pcm_writei(pcm_play_handle, data_out, frames);
  snd_pcm_writei(pcm_play_handle, data_out, frames);

	//Note: the virtual cable samples queue should be flushed at the start of tx
 	qloop.stall = 1;

  debug("sound_loop: starting sound thread",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

  while(sound_thread_continue && !shutdown_flag){

		//restart the pcm capture if there is an error reading the samples
		//this is opened as a blocking device, hence we derive accurate timing 

		last_time = gettime_now.tv_nsec/1000;

  
    debug("sound_loop: entering while pcmreturn",DEBUG_LEVEL_CRAZY_VERBOSE);

		while ((pcmreturn = snd_pcm_readi(pcm_capture_handle, data_in, frames)) < 0){
			snd_pcm_prepare(pcm_capture_handle);
			//putchar('=');
		}

    debug("sound_loop: exiting while pcmreturn",DEBUG_LEVEL_CRAZY_VERBOSE);

		i = 0; 
		j = 0;

		int ret_card = pcmreturn;
    if (use_virtual_cable){

			//printf(" we have %d in qloop, writing now\n", q_length(&qloop));
			// if don't we have enough to last two iterations loop back...
			if (q_length(&qloop) < pcmreturn){
       //				puts(" skipping");
				continue;
			}
	
			//copy 1024 samples from the queue.
			i = 0;
			j = 0;
			
			for (int samples  = 0; samples < 1024; samples++){
				int32_t s = q_read(&qloop);
				input_i[j] = input_q[j] = s;
				j++; 
			}
			played_samples += 1024;
		}
		else {
			while (i < ret_card){
				input_i[i] = data_in[j++]/2;
				input_q[i] = data_in[j++]/2;
				i++;
			}
		}

		//printf("%d %ld %d\n", count++, nsamples, pcmreturn);

		debug("sound_loop: calling sound_process()",DEBUG_LEVEL_CRAZY_VERBOSE);
			
		sound_process(input_i, input_q, output_i, output_q, ret_card);

		i = 0; 
		j = 0;	
		while (i < ret_card){
			data_out[j++] = output_i[i];
			data_out[j++] = output_q[i++];
		}

		/*
			// This is the original pcm play write routine, now commented out.
		    while ((pcmreturn = snd_pcm_writei(pcm_play_handle, 
					data_out, frames)) < 0) {
		       snd_pcm_prepare(pcm_play_handle);
		    }
		*/

		// This is the new pcm play write routine

		int framesize = ret_card;
		int offset = 0;
			
		debug("sound_loop: entering while(framesize > 0) 1",DEBUG_LEVEL_CRAZY_VERBOSE);	
		while(framesize > 0){
			pcmreturn = snd_pcm_writei(pcm_play_handle, data_out + offset, framesize);
			if((pcmreturn < 0) && (pcmreturn != -11)){	// also ignore "temporarily unavailable" errors
				// Handle an error condition from the snd_pcm_writei function
				play_write_error++;
				if (play_write_error % 10000 == 0){
					sprintf(debug_text,"sound_loop: play PCM write error:%s count:%d\n",snd_strerror(pcmreturn), play_write_error++);
					debug(debug_text,DEBUG_LEVEL_FREQUENT_NOISY);
				}
				snd_pcm_prepare(pcm_play_handle);		
			}
			
			if(pcmreturn >= 0){
				// Calculate remaining number of samples to be sent and new position in sample array.
				// If all the samples were processed by the snd_pcm_writei function then framesize will be
				// zero and the while() loop will end.
				framesize -= pcmreturn;
				offset += (pcmreturn * 2);
			}
		} // while(framesize > 0){
		// End of new pcm play write routine


		//decimate the line out to half, ie from 96000 to 48000
		//play the received data (from left channel) to both of line out
			
		int jj = 0;
		int ii = 0;
		debug("sound_loop: entering while (ii < ret_card)",DEBUG_LEVEL_CRAZY_VERBOSE);
		while (ii < ret_card){
			line_out[jj++] = output_i[ii] / LOOPBACK_LEVEL_DIVISOR;  // Left Channel. Reduce audio level to FLDIGI a bit
			line_out[jj++] = output_i[ii] / LOOPBACK_LEVEL_DIVISOR;  // Right Channel. Note: FLDIGI does not use the this channel.
			// The right channel can be used to output other integer values such as AGC, for capture by an
			// application such as audacity.
			ii += 2;	// Skip a pair of samples to account for the 96K sample to 48K sample rate change.
		}

		/*
			// This is the original pcm loopback write routine, now commented out.
		    while((pcmreturn = snd_pcm_writei(loopback_play_handle, 
					 line_out, jj)) < 0){
					 //printf("loopback rx error: %s\n", snd_strerror(pcmreturn));
		       snd_pcm_prepare(loopback_play_handle);
					//puts("preparing loopback");
		    }
		*/    

		// This is the new pcm loopback write routine
		framesize = (ret_card + 1) /2;		// only writing half the number of samples because of the slower channel rate
		offset = 0;

	  debug("sound_loop: entering while(framesize > 0) 2",DEBUG_LEVEL_CRAZY_VERBOSE);	
		while(framesize > 0){
			pcmreturn = snd_pcm_writei(loopback_play_handle, line_out + offset, framesize);
			if(pcmreturn < 0){
				loopback_write_error++;
				if (((loopback_write_error < 11) || ((loopback_write_error < 100) && (loopback_write_error % 10 == 0)) || ((loopback_write_error < 1000) && (loopback_write_error % 100 == 0)) || (loopback_write_error % 10000 == 0)) && !supress_loopback_pcm_errors) {
	        sprintf(debug_text,"sound_loop: loopback PCM write error:%s count:%d",snd_strerror(pcmreturn), loopback_write_error);
	        debug(debug_text,DEBUG_LEVEL_FREQUENT_NOISY);
	      }
				// Handle an error condition from the snd_pcm_writei function
				snd_pcm_prepare(loopback_play_handle);

			}
			
			if(pcmreturn >= 0){
				// Calculate remaining number of samples to be sent and new position in sample array.
				// If all the samples were processed by the snd_pcm_writei function then framesize will be
				// zero and the while() loop will end.	
				framesize -= pcmreturn;
				offset += (pcmreturn * 2);
			}
		} //while(framesize > 0){
		// End of new pcm loopback write routine	
		
	    
			//played_samples += pcmreturn;
  
  }  // while(sound_thread_continue && !shutdown_flag)
 
	//fclose(pf);
  debug("sound_loop: ending sound thread",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
}

// ---------------------------------------------------------------------------------------


int loopback_loop(){

	int32_t		*line_in, *line_out, *data_in, *data_out, 
						*input_i, *output_i, *input_q, *output_q;
  int pcmreturn, i, j, loopreturn;
  short s1, s2;
  int frames;

  debug("loopback_loop: starting loopback thread",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	//we allocate enough for two channels of int32_t sized samples	
  data_in = (int32_t *)malloc(buff_size * 2);
  frames = buff_size / 8;
  snd_pcm_prepare(loopback_capture_handle);

  debug("loopback_loop: entering while(sound_thread_continue)",DEBUG_LEVEL_CRAZY_VERBOSE);
  
  while(sound_thread_continue && !shutdown_flag) {

		//restart the pcm capture if there is an error reading the samples
		//this is opened as a blocking device, hence we derive accurate timing 

		last_time = gettime_now.tv_nsec/1000;

		while ((pcmreturn = snd_pcm_readi(loopback_capture_handle, data_in, frames/2)) < 0){
			snd_pcm_prepare(loopback_capture_handle);
			//putchar('=');
		}
		i = 0; 
		j = 0;
		int ret_card = pcmreturn;

		//fill up a local buffer, take only the left channel	
		i = 0; 
		j = 0;	
		for (int i = 0; i < pcmreturn; i++){
			q_write(&qloop, data_in[j]/64);
			q_write(&qloop, data_in[j]/64);
			j += 2;
		}
		nsamples += j;

		clock_gettime(CLOCK_MONOTONIC, &gettime_now);
		if (gettime_now.tv_sec != last_sec){
			if(use_virtual_cable)
      //			printf("######sampling rate %d/%d\n", played_samples, nsamples);
			last_sec = gettime_now.tv_sec;
			nsamples = 0;
			played_samples = 0;
			count = 0;
		}

  }
  debug("loopback_loop: ending loopback thread",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
}

// ---------------------------------------------------------------------------------------



void *sound_thread_function(void *ptr){


	char *device = (char *)ptr;
	struct sched_param sch;

	//switch to maximum priority
	sch.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_setschedparam(sound_thread, SCHED_FIFO, &sch);
 	//printf("opening %s sound card\n", device);	
	if (sound_start_play(device)){
		sprintf(debug_text,"sound_thread_function: error opening play device");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return NULL;
	}

	if (sound_start_capture(device)){
		sprintf(debug_text,"sound_thread_function: error opening capture device");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return NULL;
	}

	if(sound_start_loopback_play(LOOPBACK_PLAY)){
		sprintf(debug_text,"sound_thread_function: error opening loopback play device");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return NULL;
	}

	sound_thread_continue = 1;
	sound_loop();
	sound_stop();
}

// ---------------------------------------------------------------------------------------


void *loopback_thread_function(void *ptr){

	struct sched_param sch;

	//switch to maximum priority
	sch.sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_setschedparam(loopback_thread, SCHED_FIFO, &sch);
//	printf("loopback thread is %x\n", loopback_thread);
//  printf("opening loopback on plughw:1,0 sound card\n");	

	if (sound_start_loopback_capture(LOOPBACK_CAPTURE)){
		sprintf(debug_text,"loopback_thread_function: error opening loopback capture device");
		debug(debug_text,DEBUG_LEVEL_STDERR);
		return NULL;
	}
	sound_thread_continue = 1;
	loopback_loop();
	sound_stop();
}

// ---------------------------------------------------------------------------------------


int sound_thread_start(char *device){

  sprintf(debug_text,"sound_thread_start: starting %s", device);
  debug(debug_text,DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);

	q_init(&qloop, 10240);
 	qloop.stall = 1;
	pthread_create(&sound_thread, NULL, sound_thread_function, (void*)device);
	sleep(1);
	pthread_create(&loopback_thread, NULL, loopback_thread_function, (void*)device);
}

// ---------------------------------------------------------------------------------------


void sound_thread_stop(){
	debug("sound_thread_stop: called",DEBUG_LEVEL_BASIC_LESS_INFORMATIVE);
	sound_thread_continue = 0;
}

// ---------------------------------------------------------------------------------------


void sound_input(int loop){

  if (loop){
    use_virtual_cable = 1;
	} else {
    use_virtual_cable = 0;
	}

}

// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------
// ---------------------------------------------------------------------------------------


#if !defined(COMPILING_EVERYTHING)

	void sound_process(int32_t *input_i, int32_t *input_q, int32_t *output_i, int32_t *output_q, int n_samples){
	 
		for (int i = 0; i < n_samples; i++){
			output_i[i] = input_i[i];
			output_q[i] = input_q[i];
		}	

	}

	void main(int argc, char **argv){
		sound_thread_start(TEST_STANDALONE_COMPILE_THREAD_START);
		sleep(10);
		sound_thread_stop();
		sleep(10);
	}

#endif //#if !defined(COMPILING_EVERYTHING)

