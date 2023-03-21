#if !defined(_sound_h_)

#define _sound_h_


#define CONTROL_DEFAULT 0
#define CONTROL_CAPTURE_SWITCH_ALL 1
#define CONTROL_PLAYBACK_SWITCH_ALL 2
#define CONTROL_CAPTURE_VOLUME_ALL 3
#define CONTROL_PLAYBACK_VOLUME_ALL 4
#define CONTROL_ENUM_ITEM 5

int supress_loopback_pcm_errors;

int sound_thread_start(char *device);
void sound_process(
	int32_t *input_rx, int32_t *input_mic, 
	int32_t *output_speaker, int32_t *output_tx, 
	int n_samples);
void sound_thread_stop();
void sound_mixer(char *card_name, char *element, int control, int set_value);
void sound_input(int loop);
void setup_audio_codec();

#endif //!defined(_sound_h_)