// main_example.c
// 使用新架构的示例代码

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "rec_api.h"
#include "rec_interface.h"
#include "play.h"

/*============================================================================
 * 示例：使用新架构的录制和播放功能
 *============================================================================*/

// 播放输出回调 - 将数据发送到ALSA播放
static uint8_t play_output_callback(const int16_t *buf, uint16_t len, void *user_data) {
    // 通过play.h的PlayCallback发送数据
    PlayCallback((uint16_t*)buf);
    return 0;
}

// 示例1：简单录制（单数据源）
void example_simple_record(void) {
    printf("\n=== Example 1: Simple Record ===\n");

    // 初始化API
    rec_api_init();

    // 添加IIS输入源（模拟麦克风）
    rec_source_config_t iis_source = rec_interface_create_iis_input(100);  // 音量100%
    rec_api_recorder_add_source(&iis_source);

    // 设置EMMC输出（保存到文件）
    rec_api_recorder_set_output("test_record.wav");

    // 启动录制
    rec_api_recorder_start();

    // 模拟录制10秒
    for (int i = 0; i < 10000; i++) {
        rec_api_recorder_process();
        usleep(1000);  // 1ms
    }

    // 停止录制
    rec_api_recorder_stop();

    // 清理
    rec_api_deinit();

    printf("Recording completed: test_record.wav\n");
}

// 示例2：多数据源混音录制
void example_multi_source_record(void) {
    printf("\n=== Example 2: Multi-Source Mix Record ===\n");

    // 初始化API
    rec_api_init();

    // 添加多个输入源
    rec_source_config_t iis_source1 = rec_interface_create_iis_input(80);  // 音量80%
    rec_api_recorder_add_source(&iis_source1);

    rec_source_config_t iis_source2 = rec_interface_create_iis_input(60);  // 音量60%
    rec_api_recorder_add_source(&iis_source2);

    // 设置混音模式为平均混音
    rec_api_recorder_set_mix_mode(REC_MIX_MODE_AVG);

    // 设置输出
    rec_api_recorder_set_output("mixed_record.wav");

    // 启动录制
    rec_api_recorder_start();

    // 模拟录制5秒
    for (int i = 0; i < 5000; i++) {
        rec_api_recorder_process();
        usleep(1000);
    }

    // 停止录制
    rec_api_recorder_stop();
    rec_api_deinit();

    printf("Mixed recording completed: mixed_record.wav\n");
}

// 示例3：播放文件（通过回调输出）
void example_play_file(void) {
    printf("\n=== Example 3: Play File with Callback Output ===\n");

    // 初始化API
    rec_api_init();

    // 初始化音频播放设备
    init_audio_device(16, 2, REC_SAMPLE_RATE);

    // 设置播放输入源（从EMMC文件读取）
    rec_api_player_set_input("test_record.wav");

    // 设置播放输出回调（发送到ALSA）
    rec_api_player_set_output_callback(play_output_callback, NULL);

    // 启动播放
    rec_api_player_start();

    // 模拟播放10秒
    for (int i = 0; i < 10000; i++) {
        rec_api_player_process();
        usleep(1000);
    }

    // 停止播放
    rec_api_player_stop();
    rec_api_deinit();

    printf("Playback completed\n");
}

// 示例4：边录边播（监听模式）
void example_monitor_mode(void) {
    printf("\n=== Example 4: Monitor Mode (Record + Play) ===\n");

    // 初始化API
    rec_api_init();

    // 初始化音频播放设备
    init_audio_device(16, 2, REC_SAMPLE_RATE);

    // 配置录制器
    rec_source_config_t iis_input = rec_interface_create_iis_input(100);
    rec_api_recorder_add_source(&iis_input);
    rec_api_recorder_set_output("monitor.wav");

    // 启动录制
    rec_api_recorder_start();

    // 模拟边录边播5秒
    int16_t play_buffer[REC_FRAME_SAMPLES * REC_CHANNELS];
    for (int i = 0; i < 5000; i++) {
        // 处理录制
        rec_api_recorder_process();

        // 获取录制数据用于监听播放
        // （这里可以从recorder的data_buf获取，或者添加数据就绪回调）

        usleep(1000);
    }

    // 停止录制
    rec_api_recorder_stop();
    rec_api_deinit();

    printf("Monitor mode completed\n");
}

// 示例5：自定义回调数据源
static uint16_t custom_read_callback(int16_t *buf, uint16_t samples, void *user_data) {
    // 生成正弦波测试信号
    static float phase = 0.0f;
    float freq = 440.0f;  // A4音符
    float sample_rate = REC_SAMPLE_RATE;

    for (uint16_t i = 0; i < samples; i++) {
        int16_t sample = (int16_t)(sinf(phase) * 16000.0f);
        buf[i * REC_CHANNELS] = sample;      // 左声道
        buf[i * REC_CHANNELS + 1] = sample;  // 右声道

        phase += 2.0f * 3.14159f * freq / sample_rate;
        if (phase > 2.0f * 3.14159f) phase -= 2.0f * 3.14159f;
    }

    return samples;
}

void example_custom_source(void) {
    printf("\n=== Example 5: Custom Callback Source ===\n");

    // 初始化API
    rec_api_init();

    // 创建自定义回调数据源
    rec_source_config_t custom_source = rec_interface_create_custom_source(
        custom_read_callback,  // 读取回调
        NULL,                  // 写入回调（不需要）
        NULL,                  // 启动回调
        NULL,                  // 停止回调
        NULL,                  // 用户数据
        100                    // 音量
    );

    // 添加到录制器
    rec_api_recorder_add_source(&custom_source);
    rec_api_recorder_set_output("tone.wav");

    // 启动录制（实际是生成正弦波）
    rec_api_recorder_start();

    // 生成3秒音调
    for (int i = 0; i < 3000; i++) {
        rec_api_recorder_process();
        usleep(1000);
    }

    // 停止
    rec_api_recorder_stop();
    rec_api_deinit();

    printf("Tone generated: tone.wav\n");
}

int main(int argc, char *argv[]) {
    printf("BanRec - New Architecture Examples\n");
    printf("===================================\n");

    if (argc < 2) {
        printf("\nUsage: %s <example_number>\n", argv[0]);
        printf("  1 - Simple Record\n");
        printf("  2 - Multi-Source Mix Record\n");
        printf("  3 - Play File\n");
        printf("  4 - Monitor Mode\n");
        printf("  5 - Custom Source\n");
        return 1;
    }

    int example = atoi(argv[1]);

    switch (example) {
        case 1:
            example_simple_record();
            break;
        case 2:
            example_multi_source_record();
            break;
        case 3:
            example_play_file();
            break;
        case 4:
            example_monitor_mode();
            break;
        case 5:
            example_custom_source();
            break;
        default:
            printf("Invalid example number: %d\n", example);
            return 1;
    }

    return 0;
}
