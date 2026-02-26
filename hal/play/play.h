#ifndef _PLAY_H__
#define _PLAY_H__

#include <stdint.h>
#define PCMDEVICE "default"
#define SAMPLERATE 48000
#define MAXCHANNELS 2
#define BYTESPERSAMPLE (sizeof(short) * MAXCHANNELS) // 2 bytes per sample * number of channels

#define BUFFER_SIZE ((SAMPLERATE/1000)) // Number of samples per millisecond
#define MAXRING_BUFFER_SIZE (BUFFER_SIZE * 2) // Ring buffer size (double buffering)


void init_audio_device(uint8_t width, uint8_t ch_num,uint16_t samplerate);
void PlayCallback(uint16_t* data);
void bg_play_loop();
void bg_play_enable(uint8_t enable);
uint8_t bg_play_get_state();



#endif 