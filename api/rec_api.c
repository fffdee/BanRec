// rec_api.c
// API层实现 - 提供简洁易用的录制和播放接口

#include "rec_api.h"
#include "rec_interface.h"
#include "rec_err_handle.h"
#include <string.h>

/*============================================================================
 * 内部变量
 *============================================================================*/

// 录制器和播放器实例
static rec_stream_t g_recorder = {0};
static rec_stream_t g_player = {0};

// 录制器和播放器配置
static rec_recorder_config_t g_recorder_config = {0};
static rec_player_config_t g_player_config = {0};

// 初始化标志
static uint8_t g_recorder_inited = 0;
static uint8_t g_player_inited = 0;

/*============================================================================
 * 模块初始化
 *============================================================================*/

uint8_t rec_api_init(void) {
    // 初始化接口层
    rec_interface_init();

    // 初始化录制器配置（默认无输入源，需要手动添加）
    memset(&g_recorder_config, 0, sizeof(rec_recorder_config_t));
    g_recorder_config.mix_mode = REC_MIX_MODE_ADD;  // 默认叠加混音
    g_recorder_config.input_source_count = 0;

    // 初始化播放器配置（默认无输入源，需要手动设置）
    memset(&g_player_config, 0, sizeof(rec_player_config_t));

    LOG_INFO("[API_INIT] API layer initialized\n");

    return REC_ERR_OK;
}

uint8_t rec_api_deinit(void) {
    // 停止录制和播放
    if (g_recorder_inited) {
        rec_api_recorder_stop();
    }
    if (g_player_inited) {
        rec_api_player_stop();
    }

    // 去初始化接口层
    rec_interface_deinit();

    g_recorder_inited = 0;
    g_player_inited = 0;

    LOG_INFO("[API_DEINIT] API layer deinitialized\n");

    return REC_ERR_OK;
}

/*============================================================================
 * 录制器API实现
 *============================================================================*/

uint8_t rec_api_recorder_add_source(rec_source_config_t *source) {
    if (!source) {
        return rec_err(REC_CONFIG_NO_SOURCE_IN);
    }

    if (g_recorder_config.input_source_count >= REC_INPUT_SOURCE_MAX) {
        return rec_err(REC_ERR_UNSUPPORTED);
    }

    g_recorder_config.input_sources[g_recorder_config.input_source_count] = *source;
    g_recorder_config.input_source_count++;

    LOG_INFO("[API_RECORDER] Added input source, total: %d\n",
             g_recorder_config.input_source_count);

    return REC_ERR_OK;
}

uint8_t rec_api_recorder_set_output(const char *filename) {
    // 创建EMMC输出源
    g_recorder_config.output_source = rec_interface_create_emmc_record_output(filename);

    LOG_INFO("[API_RECORDER] Set output target\n");

    return REC_ERR_OK;
}

uint8_t rec_api_recorder_set_output_from_node(
    rec_iface_type_t node_type,
    uint8_t volume,
    void *user_data
) {
    g_recorder_config.output_source = rec_interface_create_source_from_node(
        node_type, volume, user_data);
    
    if (g_recorder_config.output_source.type == REC_SOURCE_NONE) {
        LOG_INFO("[API_RECORDER] Failed to set output from node type: %d\n", node_type);
        return rec_err(REC_CONFIG_NO_SOURCE_OUT);
    }
    
    LOG_INFO("[API_RECORDER] Set output from node type: %d\n", node_type);
    return REC_ERR_OK;
}

void rec_api_recorder_set_mix_mode(rec_mix_mode_t mode) {
    g_recorder_config.mix_mode = mode;
    LOG_INFO("[API_RECORDER] Set mix mode: %d\n", mode);
}

uint8_t rec_api_recorder_start(void) {
    // 如果未初始化，先初始化
    if (!g_recorder_inited) {
        if (rec_recorder_init(&g_recorder, &g_recorder_config) != REC_ERR_OK) {
            return rec_err(REC_INIT_FAIL);
        }
        g_recorder_inited = 1;
    }

    // 启动录制
    uint8_t ret = rec_stream_start(&g_recorder, NULL);
    if (ret == REC_ERR_OK) {
        LOG_INFO("[API_RECORDER] Recorder started\n");
    }

    return ret;
}

uint8_t rec_api_recorder_stop(void) {
    if (!g_recorder_inited) {
        return REC_ERR_OK;
    }

    uint8_t ret = rec_stream_stop(&g_recorder);
    if (ret == REC_ERR_OK) {
        LOG_INFO("[API_RECORDER] Recorder stopped\n");
    }

    return ret;
}

uint8_t rec_api_recorder_pause(void) {
    if (!g_recorder_inited) {
        return rec_err(REC_ERR_STATE);
    }

    return rec_stream_pause(&g_recorder);
}

uint8_t rec_api_recorder_resume(void) {
    if (!g_recorder_inited) {
        return rec_err(REC_ERR_STATE);
    }

    return rec_stream_resume(&g_recorder);
}

uint8_t rec_api_recorder_process(void) {
    if (!g_recorder_inited) {
        return REC_ERR_OK;
    }

    return rec_stream_process(&g_recorder);
}

void rec_api_recorder_update(void) {
    if (g_recorder_inited) {
        rec_stream_update(&g_recorder);
    }
}

/*============================================================================
 * 播放器API实现
 *============================================================================*/

uint8_t rec_api_player_set_input(const char *filename) {
    if (!filename) {
        return rec_err(REC_CONFIG_NO_SOURCE_IN);
    }

    // 创建EMMC播放输入源
    g_player_config.input_source = rec_interface_create_emmc_play_input(filename);

    LOG_INFO("[API_PLAYER] Set input source: %s\n", filename);

    return REC_ERR_OK;
}

void rec_api_player_set_output_callback(rec_data_output_cb_t output_cb, void *user_data) {
    g_player_config.output_cb = output_cb;
    g_player_config.output_user_data = user_data;

    LOG_INFO("[API_PLAYER] Set output callback\n");
}

uint8_t rec_api_player_start(void) {
    // 如果未初始化，先初始化
    if (!g_player_inited) {
        if (rec_player_init(&g_player, &g_player_config) != REC_ERR_OK) {
            return rec_err(REC_INIT_FAIL);
        }
        g_player_inited = 1;
    }

    // 启动播放
    uint8_t ret = rec_stream_start(&g_player, NULL);
    if (ret == REC_ERR_OK) {
        LOG_INFO("[API_PLAYER] Player started\n");
    }

    return ret;
}

uint8_t rec_api_player_stop(void) {
    if (!g_player_inited) {
        return REC_ERR_OK;
    }

    uint8_t ret = rec_stream_stop(&g_player);
    if (ret == REC_ERR_OK) {
        LOG_INFO("[API_PLAYER] Player stopped\n");
    }

    return ret;
}

uint8_t rec_api_player_pause(void) {
    if (!g_player_inited) {
        return rec_err(REC_ERR_STATE);
    }

    return rec_stream_pause(&g_player);
}

uint8_t rec_api_player_resume(void) {
    if (!g_player_inited) {
        return rec_err(REC_ERR_STATE);
    }

    return rec_stream_resume(&g_player);
}

uint8_t rec_api_player_process(void) {
    if (!g_player_inited) {
        return REC_ERR_OK;
    }

    return rec_stream_process(&g_player);
}

uint16_t rec_api_player_get_data(int16_t *buf, uint16_t len) {
    if (!g_player_inited) {
        return 0;
    }

    return rec_stream_get_play_data(&g_player, buf, len);
}

void rec_api_player_update(void) {
    if (g_player_inited) {
        rec_stream_update(&g_player);
    }
}