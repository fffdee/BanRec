******************************************************
  funstion: Main component of BanRec - 录制播放演示程序
    author  : BanGO
    
  功能说明:
  - 按 'r' 开始录制 (IIS输入 -> 文件输出)
  - 按 't' 停止录制
  - 按 'p' 播放录制内容 (文件输入 -> IIS输出)
  - 按空格键 切换合成音播放/暂停
  
  配置方式:
  - 使用节点表系统自由配置输入输出源
  - 支持多输入源混音
  - 支持不同类型的接口节点(IIS, EMMC, NOR Flash等)
******************************************************/

#include <termios.h>
#include <stdio.h>
#include <ev.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdint.h>
#include <signal.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <unistd.h>
#include <sched.h>
#include "rec_config.h"
#define ENABLE_KEYBOARD_INPUT 1
#if ENABLE_KEYBOARD_INPUT
#include "input_handle.h"
#endif
#include "play.h"
#include "rec_api.h"
#include "rec_interface.h"
#include <math.h>
#include <string.h>
 
#define PI 3.14159265358979323846
#define SAMPLE_RATE 48000
#define DURATION_SEC 1
#define NUM_NOTES 8
#define RING_BUF_SIZE (SAMPLE_RATE * 2) // 2秒缓冲
#define FEED_SAMPLES 48

static const double note_freqs[NUM_NOTES] = {261.63, 293.66, 329.63, 349.23, 392.00, 440.00, 493.88, 523.25};

// 录制相关变量
static int recording = 0;  // 0=停止，1=录制中
static int playing_recording = 0;  // 0=停止，1=播放录制内容
static const char *record_filename = "recorded.wav";

void generate_sine_wave(short* buffer, int samples, double freq, int samplerate) {
    for (int i = 0; i < samples; ++i) {
        double t = (double)i / samplerate;
        buffer[i] = (short)(sin(2 * PI * freq * t) * 32767 * 0.2);
    }
}

// 录制播放回调函数
void recording_play_callback(const int16_t *buf, uint16_t len, void *user_data) {
    if (buf && len > 0) {
        // 将录制的数据发送到播放系统
        PlayCallback((uint16_t*)buf);
    }
}

// 配置示例函数 - 展示如何使用节点表配置不同的输入输出组合
void configure_recording_example(void) {
    printf("=== 录制配置示例 ===\n");
    
    // 示例1：单IIS输入 -> EMMC输出
    printf("配置1: IIS输入 -> EMMC文件录制\n");
    rec_source_config_t iis_in = rec_interface_create_source_from_node(REC_IFACE_IIS_IN, 100, NULL);
    rec_api_recorder_add_source(&iis_in);
    rec_api_recorder_set_output_from_node(REC_IFACE_EMMC_OUT, 100, (void*)"single_iis.wav");
    
    // 示例2：多输入混音 -> EMMC输出
    printf("配置2: 多IIS输入混音 -> EMMC文件录制\n");
    // 清除之前的配置
    // rec_api_recorder_clear_sources(); // 需要添加这个API
    
    // 示例3：IIS输入 -> NOR Flash输出（如果实现）
    // printf("配置3: IIS输入 -> NOR Flash录制\n");
    // rec_source_config_t nor_out = rec_interface_create_source_from_node(REC_IFACE_NOR_FLASH_OUT, 100, NULL);
    // rec_api_recorder_set_output_from_node(REC_IFACE_NOR_FLASH_OUT, 100, NULL);
    
    printf("=== 配置完成 ===\n");
}

struct timeval start_time;
volatile int feed_flag = 0;
volatile int ms_counter = 0;
volatile int note_idx = 0;
short note_buf[SAMPLE_RATE];
int note_pos = 0;

/* 
 * 定时器中断处理函数 (1ms)
 * 只负责轻量级状态更新,不处理音频
 */
void timer_handler(int signum)
{
    feed_flag = 1;
    ms_counter++;
    
    // 调用录制和播放的定时更新
    if (recording) {
        rec_api_recorder_update();
    }
    if (playing_recording) {
        rec_api_player_update();
    }
    
    if (ms_counter >= 1000) {
        ms_counter = 0;
        note_idx = (note_idx + 1) % NUM_NOTES;
        note_pos = 0;
        generate_sine_wave(note_buf, SAMPLE_RATE, note_freqs[note_idx], SAMPLE_RATE);
    }
    feed_flag = 1;
}

char keyinput;
void time_init()
{
    struct itimerval itv;
    itv.it_interval.tv_sec = 0;
    itv.it_interval.tv_usec = 1000;
    itv.it_value.tv_sec = 0;
    itv.it_value.tv_usec = 1000;

    signal(SIGALRM, timer_handler);
    setitimer(ITIMER_REAL, &itv, NULL);
    gettimeofday(&start_time, NULL);
}

void Init()
{
    time_init();
    
    // 初始化录制API
    if (rec_api_init() != 0) {
        printf("Failed to initialize recording API\n");
        exit(1);
    }
    
    // 显示配置示例
    configure_recording_example();
    
    // 实际配置 - 用户可以根据需要修改
    printf("\n=== 实际配置 ===\n");
    
    // 示例：配置录制器使用IIS输入源和EMMC输出源
    // 用户可以根据需要修改这些配置
    
    // 添加IIS输入源（录音输入）
    rec_source_config_t input_source = rec_interface_create_source_from_node(
        REC_IFACE_IIS_IN, 100, NULL);  // 100%音量
    if (input_source.type != REC_SOURCE_NONE) {
        if (rec_api_recorder_add_source(&input_source) != 0) {
            printf("Failed to add recording input source\n");
            exit(1);
        }
        printf("Added IIS input source for recording\n");
    } else {
        printf("Failed to create IIS input source\n");
        exit(1);
    }
    
    // 示例：添加第二个输入源（如果需要混音）
    // rec_source_config_t input_source2 = rec_interface_create_source_from_node(
    //     REC_IFACE_IIS_IN, 80, NULL);  // 80%音量
    // if (input_source2.type != REC_SOURCE_NONE) {
    //     rec_api_recorder_add_source(&input_source2);
    //     printf("Added second IIS input source for mixing\n");
    // }
    
    // 设置EMMC输出源（录制到文件）
    if (rec_api_recorder_set_output_from_node(REC_IFACE_EMMC_OUT, 100, (void*)record_filename) != 0) {
        printf("Failed to set recording output\n");
        exit(1);
    }
    printf("Set EMMC output for recording to: %s\n", record_filename);
    
    // 设置混音模式（如果有多个输入源）
    rec_api_recorder_set_mix_mode(REC_MIX_MODE_ADD);  // 叠加混音
    
    // 配置播放器输入源（从文件播放）
    rec_api_player_set_output_callback(recording_play_callback, NULL);
    printf("Configured player with callback output\n");
    
#if ENABLE_KEYBOARD_INPUT
    BG_input_handle.KeyBoardInit();
#endif
}

int main(void)
{
    Init();
    init_audio_device(16, 1, SAMPLE_RATE);
    bg_play_enable(1);

    // 先生成第一个音符
    generate_sine_wave(note_buf, SAMPLE_RATE, note_freqs[note_idx], SAMPLE_RATE);
    note_pos = 0;
    short feed_buf[FEED_SAMPLES];

    int playing = 1; // 1=播放，0=暂停
    int missed = 0;
    while (1)
    {
        // 处理录制
        if (recording) {
            rec_api_recorder_process();
        }
        
        // 处理播放录制内容
        if (playing_recording) {
            rec_api_player_process();
        }
        
        if (feed_flag && playing && !playing_recording) {  // 当播放录制内容时，不播放合成音
            feed_flag = 0;
            missed++;
            // 一次性补喂 missed*FEED_SAMPLES
            for (int m = 0; m < missed; ++m) {
                int remain = SAMPLE_RATE - note_pos;
                int copy = (remain >= FEED_SAMPLES) ? FEED_SAMPLES : remain;
                memcpy(feed_buf, &note_buf[note_pos], copy * sizeof(short));
                if (copy < FEED_SAMPLES) {
                    memset(feed_buf + copy, 0, (FEED_SAMPLES - copy) * sizeof(short));
                }
                PlayCallback((uint16_t*)feed_buf);
                note_pos += copy;
                if (note_pos >= SAMPLE_RATE) note_pos = 0;
            }
            missed = 0;
        }

#if ENABLE_KEYBOARD_INPUT
        /* 处理键盘输入 (非阻塞) */
        keyinput = BG_input_handle.KeyBoardLoop();
        
        /* 只在有实际输入时打印 */
        if (keyinput > 0 && keyinput != 10) {
            printf("data is %d\n", keyinput);
            
            if (keyinput == 32) { // 空格键切换播放/暂停
                playing = !playing;
                printf("%s\n", playing ? "播放" : "暂停");
            }
            else if (keyinput == 'r' || keyinput == 'R') { // r键开始录制
                if (!recording) {
                    if (rec_api_recorder_start() == 0) {
                        recording = 1;
                        playing_recording = 0;  // 停止播放录制内容
                        printf("开始录制...\n");
                    } else {
                        printf("录制启动失败\n");
                    }
                }
            }
            else if (keyinput == 't' || keyinput == 'T') { // t键停止录制
                if (recording) {
                    if (rec_api_recorder_stop() == 0) {
                        recording = 0;
                        printf("录制停止\n");
                    } else {
                        printf("录制停止失败\n");
                    }
                }
            }
            else if (keyinput == 'p' || keyinput == 'P') { // p键播放录制内容
                if (!recording && !playing_recording) {  // 只有在不录制时才能播放
                    if (rec_api_player_set_input(record_filename) == 0 &&
                        rec_api_player_start() == 0) {
                        playing_recording = 1;
                        playing = 0;  // 暂停合成音播放
                        printf("播放录制内容...\n");
                    } else {
                        printf("播放启动失败\n");
                    }
                } else if (playing_recording) {
                    // 如果正在播放，则停止
                    if (rec_api_player_stop() == 0) {
                        playing_recording = 0;
                        playing = 1;  // 恢复合成音播放
                        printf("停止播放录制内容\n");
                    }
                }
            }
        }
#endif
    }
    
    // 清理资源
    rec_api_deinit();
    Deinit_audio_device();
    
    return 0;
}
