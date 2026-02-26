#include "play.h"
#include <alsa/asoundlib.h>
#include <math.h>
#include <unistd.h>
#include <sched.h>

typedef struct {
    short *buffer;           // 指向缓冲区的指针
    int size;                // 缓冲区大小（以样本为单位）
    volatile int play_index; // 播放索引
    volatile int buffer_index; // 缓冲索引
} RingBuffer;

typedef struct
{
    /* data */
    
    uint8_t enable;
    uint8_t init_flag;
    short *PlayBuffer; 
    uint8_t ch;
    uint16_t samplerate;
    uint16_t numsamples;
    RingBuffer ring_buffer;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    
    
}RunData;

RunData runData = {

    .enable = 1,
    .init_flag = 0,
    
};


volatile int buffer_index = 0;
volatile int play_index = 0;





void bg_play_enable(uint8_t enable){
    if(enable>1)enable=1;
    runData.enable = enable;
}

void PlayCallback(uint16_t* data){
    // Check if audio device is available
    if (runData.handle == NULL) {
        // Simulation mode - just return without audio output
        return;
    }

    int frames = runData.numsamples / runData.ch;
    int err;
    while ((err = snd_pcm_writei(runData.handle, data, frames)) < 0) {
        if (err == -EPIPE) {
            // EPIPE means underrun
           // printf("Underrun occurred\n");

            // Prepare the PCM device after underrun
            if ((err = snd_pcm_prepare(runData.handle)) < 0) {
                printf("Can't prepare audio device after underrun: %s\n", snd_strerror(err));
                return;  // Don't exit in simulation mode
            }
            // Optionally, you can refill the buffer here if needed
        } else if (err == -EAGAIN) {
            // Wait until the audio device is ready
            // This can happen if the buffer is full
            sched_yield();
        } else {
            printf("Write error: %s\n", snd_strerror(err));
            // Other errors, don't exit - continue in simulation mode
            return;
        }
    }
}


void bg_play_loop(){
    // Check if audio device is available
    if (runData.handle == NULL) {
        // Simulation mode - just return without audio output
        return;
    }

    // 使用结构体中的缓冲区和索引
    short *play_buffer = runData.ring_buffer.buffer;
   
        // Copy samples from the ring buffer to the play buffer
    for (int i = 0; i < runData.numsamples; i++) {
        runData.PlayBuffer[i] = play_buffer[runData.ring_buffer.play_index + i];
    }
    runData.ring_buffer.play_index = (runData.ring_buffer.play_index + runData.numsamples) % ((runData.samplerate/1000)*runData.ch);
    int frames = runData.numsamples / runData.ch;
    int err;
    
    /* 检查可用空间,避免阻塞 */
    snd_pcm_sframes_t avail = snd_pcm_avail_update(runData.handle);
    if (avail < 0) {
        if (avail == -EPIPE) {
            printf("[WARNING] Underrun detected, recovering...\n");
            snd_pcm_prepare(runData.handle);
            avail = snd_pcm_avail_update(runData.handle);
        } else {
            printf("avail_update error: %s\n", snd_strerror(avail));
            return;  // Don't continue if error
        }
    }
    
    /* 如果缓冲区空间不足,跳过这次写入(避免阻塞) */
    if (avail < frames) {
        return;  // 下次再写
    }
    
    while ((err = snd_pcm_writei(runData.handle, runData.PlayBuffer, frames)) < 0) {
        if (err == -EPIPE) {
            printf("[WARNING] Underrun occurred, recovering...\n");
            if ((err = snd_pcm_prepare(runData.handle)) < 0) {
                printf("Can't prepare audio device after underrun: %s\n", snd_strerror(err));
                return;  // Don't exit
            }
            /* 恢复后重试写入 */
        } else if (err == -EAGAIN) {
            usleep(100);  // 等待 100us 后重试
        } else {
            printf("Write error: %s\n", snd_strerror(err));
            return;  // Don't exit
        }
    }
}

uint8_t bg_play_get_state(){

    return runData.enable;
}

void init_audio_device(uint8_t width, uint8_t ch_num,uint16_t samplerate) {
   
    int err;
    uint8_t sampleformat = SND_PCM_FORMAT_S16_LE; // 默认16位

    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler failed");
        exit(1);
    }


    if ((err = snd_pcm_open(&runData.handle, PCMDEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Error opening PCM device: %s\n", snd_strerror(err));
        printf("Audio device not available, running in simulation mode\n");
        runData.handle = NULL;  // Mark as unavailable
        runData.init_flag = 1;  // Still mark as initialized for simulation
        return;  // Don't exit, continue without audio
    }

    if ((err = snd_pcm_hw_params_malloc(&runData.params)) < 0) {
        printf("Cannot allocate hardware parameter structure: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_any(runData.handle, runData.params)) < 0) {
        printf("Cannot initialize hardware parameter structure: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_access(runData.handle, runData.params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        printf("Cannot set access type: %s\n", snd_strerror(err));
        exit(1);
    }
    if(width==16) sampleformat = SND_PCM_FORMAT_S16_LE;
    else if(width==24) sampleformat = SND_PCM_FORMAT_S24_LE;
    else {
        printf("Unsupported sample width: %d\n", width);
        exit(1);
    }
    if ((err = snd_pcm_hw_params_set_format(runData.handle, runData.params, sampleformat)) < 0) {
        printf("Cannot set sample format: %s\n", snd_strerror(err));
        exit(1);
    }

    unsigned int val = samplerate;
    if ((err = snd_pcm_hw_params_set_rate_near(runData.handle, runData.params, &val, 0)) < 0) {
        printf("Cannot set sample rate: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params_set_channels(runData.handle, runData.params, ch_num)) < 0) {
        printf("Cannot set channel count: %s\n", snd_strerror(err));
        exit(1);
    }

    /* 设置缓冲区大小 - 增大以减少 underrun */
    snd_pcm_uframes_t buffer_size = (samplerate / 1000) * 16;  // 16ms 缓冲 (原来是隐式的更小值)
    if ((err = snd_pcm_hw_params_set_buffer_size_near(runData.handle, runData.params, &buffer_size)) < 0) {
        printf("Cannot set buffer size: %s\n", snd_strerror(err));
        exit(1);
    }
    
    /* 设置 period size - 每次写入的数据块大小 */
    snd_pcm_uframes_t period_size = (samplerate / 1000) * 4;  // 4ms 一个 period
    if ((err = snd_pcm_hw_params_set_period_size_near(runData.handle, runData.params, &period_size, 0)) < 0) {
        printf("Cannot set period size: %s\n", snd_strerror(err));
        exit(1);
    }

    if ((err = snd_pcm_hw_params(runData.handle, runData.params)) < 0) {
        printf("Cannot set hardware parameters: %s\n", snd_strerror(err));
        exit(1);
    }
    
    /* 打印实际配置 */
    snd_pcm_hw_params_get_buffer_size(runData.params, &buffer_size);
    snd_pcm_hw_params_get_period_size(runData.params, &period_size, 0);
    printf("ALSA configured: buffer=%lu frames (%.1fms), period=%lu frames (%.1fms)\n",
           buffer_size, (buffer_size * 1000.0) / samplerate,
           period_size, (period_size * 1000.0) / samplerate);
   
    runData.ring_buffer.size = samplerate/1000*ch_num*2;
    runData.ring_buffer.buffer = (short *)malloc(runData.ring_buffer.size * sizeof(short));
    if (runData.ring_buffer.buffer == NULL) {
        perror("Failed to allocate ring buffer");
        //return 1;
    }
    runData.ring_buffer.play_index = 0;
    runData.ring_buffer.buffer_index = 0;
    runData.ch = ch_num;
    runData.samplerate = samplerate;
    runData.numsamples = (samplerate/1000)*ch_num;

    runData.PlayBuffer = (short *)malloc(runData.numsamples * sizeof(short));
    runData.init_flag=1;

}

void Deinit_audio_device(){
    if (runData.PlayBuffer) {
        free(runData.PlayBuffer);
        runData.PlayBuffer = NULL;
    }
    if (runData.ring_buffer.buffer) {
        free(runData.ring_buffer.buffer);
        runData.ring_buffer.buffer = NULL;
    }
    if (runData.handle != NULL) {
        snd_pcm_drain(runData.handle);
        snd_pcm_close(runData.handle);
        runData.handle = NULL;
    }
    runData.init_flag = 0;
}
