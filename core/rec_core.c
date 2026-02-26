// rec_core.c
// 音频录制/播放核心实现 - 完全解耦架构
// 支持多数据源混音输入、回调机制、HAL抽象

#include "rec_core.h"
#include "rec_err_handle.h"
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

// --- 内部操作函数声明 ---
static uint8_t rec_stream_start_internal(rec_stream_t *stream, rec_file_config_t *file);
static uint8_t rec_stream_process_internal(rec_stream_t *stream);
static uint8_t rec_stream_stop_internal(rec_stream_t *stream);
static uint8_t rec_stream_pause_internal(rec_stream_t *stream);
static uint8_t rec_stream_resume_internal(rec_stream_t *stream);
static void rec_stream_update_internal(rec_stream_t *stream);

// --- 操作函数表 ---
static rec_opt_t opt_table = {
    .start = rec_stream_start_internal,
    .process = rec_stream_process_internal,
    .stop = rec_stream_stop_internal,
    .pause = rec_stream_pause_internal,
    .resume = rec_stream_resume_internal,
    .update = rec_stream_update_internal,
};

// --- 默认文件配置 ---
static char default_file_name[64] = "";
static void set_default_file_name_by_time() {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(default_file_name, sizeof(default_file_name), "%Y%m%d_%H%M%S.wav", tm_info);
}

static rec_file_config_t default_file = {
    .path = REC_DEFAULT_PATH,
    .file_name = default_file_name,
    .channels = REC_CHANNELS,
    .sample_width = REC_SAMPLE_WIDTH,
    .sample_rate = REC_SAMPLE_RATE,
    .auto_stop_ms = 0
};

// --- 混音处理函数 ---

/**
 * @brief 执行多数据源混音
 * @param stream 流对象
 * @param output 输出缓冲区
 * @param frame_samples 帧样本数
 */
static void rec_mix_sources(rec_stream_t *stream, int16_t *output, uint16_t frame_samples) {
    if (!stream || !output || stream->type != REC_TYPE_RECORDER) {
        return;
    }

    rec_recorder_config_t *recorder = &stream->config.recorder;
    uint16_t total_samples = frame_samples * REC_CHANNELS;
    uint8_t active_sources = 0;

    // 清空输出缓冲区
    memset(output, 0, total_samples * sizeof(int16_t));

    // 统计有效源数量
    for (uint8_t i = 0; i < recorder->input_source_count; i++) {
        if (recorder->input_sources[i].enabled) {
            active_sources++;
        }
    }

    if (active_sources == 0) {
        return;
    }

    // 根据混音模式处理
    switch (recorder->mix_mode) {
        case REC_MIX_MODE_NONE: {
            // 使用第一个有效源
            for (uint8_t i = 0; i < recorder->input_source_count; i++) {
                rec_source_config_t *source = &recorder->input_sources[i];
                if (source->enabled && source->read_cb) {
                    uint16_t read_samples = source->read_cb(output, frame_samples, source->user_data);
                    if (read_samples > 0) {
                        // 应用音量
                        if (source->volume != 100) {
                            float vol_scale = source->volume / 100.0f;
                            for (uint16_t j = 0; j < total_samples; j++) {
                                output[j] = (int16_t)(output[j] * vol_scale);
                            }
                        }
                        break; // 只使用第一个源
                    }
                }
            }
            break;
        }

        case REC_MIX_MODE_ADD: {
            // 叠加混音
            for (uint8_t i = 0; i < recorder->input_source_count; i++) {
                rec_source_config_t *source = &recorder->input_sources[i];
                if (source->enabled && source->read_cb) {
                    uint16_t read_samples = source->read_cb(stream->mix_buf, frame_samples, source->user_data);
                    if (read_samples > 0) {
                        // 应用音量并叠加
                        float vol_scale = source->volume / 100.0f;
                        for (uint16_t j = 0; j < total_samples; j++) {
                            int32_t mixed = output[j] + (int32_t)(stream->mix_buf[j] * vol_scale);
                            // 防止溢出
                            if (mixed > INT16_MAX) mixed = INT16_MAX;
                            if (mixed < INT16_MIN) mixed = INT16_MIN;
                            output[j] = (int16_t)mixed;
                        }
                    }
                }
            }
            break;
        }

        case REC_MIX_MODE_AVG: {
            // 平均混音
            int32_t temp_buf[REC_FRAME_SAMPLES * REC_CHANNELS] = {0};

            for (uint8_t i = 0; i < recorder->input_source_count; i++) {
                rec_source_config_t *source = &recorder->input_sources[i];
                if (source->enabled && source->read_cb) {
                    uint16_t read_samples = source->read_cb(stream->mix_buf, frame_samples, source->user_data);
                    if (read_samples > 0) {
                        float vol_scale = source->volume / 100.0f;
                        for (uint16_t j = 0; j < total_samples; j++) {
                            temp_buf[j] += (int32_t)(stream->mix_buf[j] * vol_scale);
                        }
                    }
                }
            }

            // 计算平均值
            if (active_sources > 0) {
                for (uint16_t j = 0; j < total_samples; j++) {
                    output[j] = (int16_t)(temp_buf[j] / active_sources);
                }
            } else {
                memset(output, 0, total_samples * sizeof(int16_t));
            }
            break;
        }

        default:
            break;
    }
}

// --- 录制器初始化 ---
uint8_t rec_recorder_init(rec_stream_t *stream, rec_recorder_config_t *config) {
    if (!stream || !config) {
        return rec_err(REC_CONFIG_NO_SOURCE_IN);
    }

    memset(stream, 0, sizeof(rec_stream_t));
    stream->type = REC_TYPE_RECORDER;
    stream->state = REC_STREAM_UNINIT;
    stream->opt = &opt_table;
    stream->frame_size = REC_FRAME_SAMPLES;

    // 复制配置
    stream->config.recorder = *config;

    // 设置默认文件配置
    stream->file_config = default_file;

    stream->state = REC_STREAM_READY;
    LOG_INFO("[RECORDER_INIT] Recorder initialized with %d input sources\n",
             config->input_source_count);

    return REC_ERR_OK;
}

// --- 播放器初始化 ---
uint8_t rec_player_init(rec_stream_t *stream, rec_player_config_t *config) {
    if (!stream || !config) {
        return rec_err(REC_CONFIG_NO_SOURCE_IN);
    }

    memset(stream, 0, sizeof(rec_stream_t));
    stream->type = REC_TYPE_PLAYER;
    stream->state = REC_STREAM_UNINIT;
    stream->opt = &opt_table;
    stream->frame_size = REC_FRAME_SAMPLES;

    // 复制配置
    stream->config.player = *config;

    // 设置默认文件配置
    stream->file_config = default_file;

    stream->state = REC_STREAM_READY;
    LOG_INFO("[PLAYER_INIT] Player initialized\n");

    return REC_ERR_OK;
}

// --- 流启动实现 ---
static uint8_t rec_stream_start_internal(rec_stream_t *stream, rec_file_config_t *file) {
    LOG_INFO("[START] rec_stream_start called\n");

    if (!stream) {
        return rec_err(REC_CONFIG_NO_SOURCE_IN);
    }

    if (!file) {
        set_default_file_name_by_time();
        stream->file_config = default_file;
        LOG_INFO("[START] Use default file name: %s\n", default_file_name);
    } else {
        stream->file_config = *file;
        LOG_INFO("[START] Use custom file name: %s\n", file->file_name);
    }

    stream->state = REC_STREAM_START;
    stream->current_pos_ms = 0;
    stream->total_samples = 0;
    stream->data_ready = 0;
    stream->data_request = 0;

    // 启动所有启用的数据源
    uint8_t start_count = 0;

    if (stream->type == REC_TYPE_RECORDER) {
        rec_recorder_config_t *recorder = &stream->config.recorder;

        // 启动输入源
        for (uint8_t i = 0; i < recorder->input_source_count; i++) {
            rec_source_config_t *source = &recorder->input_sources[i];
            if (source->enabled && source->start_cb) {
                if (source->start_cb(stream, source->user_data) == REC_ERR_OK) {
                    start_count++;
                    LOG_INFO("[START] Input source %d started\n", i);
                }
            }
        }

        // 启动输出源
        if (recorder->output_source.enabled && recorder->output_source.start_cb) {
            if (recorder->output_source.start_cb(stream, recorder->output_source.user_data) == REC_ERR_OK) {
                start_count++;
                LOG_INFO("[START] Output source started\n");
            }
        }

    } else if (stream->type == REC_TYPE_PLAYER) {
        rec_player_config_t *player = &stream->config.player;

        // 启动输入源
        if (player->input_source.enabled && player->input_source.start_cb) {
            if (player->input_source.start_cb(stream, player->input_source.user_data) == REC_ERR_OK) {
                start_count++;
                LOG_INFO("[START] Player input source started\n");
            }
        }
    }

    if (start_count == 0) {
        return rec_err(REC_CONFIG_NO_SOURCE_IN);
    }

    stream->state = REC_STREAM_RUNNING;
    LOG_INFO("[START] Stream started successfully, %d sources started\n", start_count);

    return REC_ERR_OK;
}

// --- 流处理实现 ---
static uint8_t rec_stream_process_internal(rec_stream_t *stream) {
    LOG_INFO("[PROCESS] rec_stream_process called\n");

    if (!stream || stream->state != REC_STREAM_RUNNING) {
        return rec_err(REC_ERR_STATE);
    }

    if (stream->type == REC_TYPE_RECORDER) {
        // 录制处理：读取输入 -> 混音 -> 写入输出
        rec_mix_sources(stream, stream->data_buf, stream->frame_size);

        // 写入输出源
        rec_recorder_config_t *recorder = &stream->config.recorder;
        if (recorder->output_source.enabled && recorder->output_source.write_cb) {
            uint16_t samples_to_write = stream->frame_size * REC_CHANNELS;
            if (recorder->output_source.write_cb(stream->data_buf, samples_to_write,
                                               recorder->output_source.user_data) != REC_ERR_OK) {
                LOG_INFO("[PROCESS] Write failed\n");
                return rec_err(REC_ERR_STATE);
            }
        }

        stream->total_samples += stream->frame_size;
        stream->data_ready = 1;

        // 调用数据就绪回调
        if (stream->on_data_ready) {
            stream->on_data_ready(stream, stream->data_buf, stream->frame_size);
        }

    } else if (stream->type == REC_TYPE_PLAYER) {
        // 播放处理：读取输入 -> 通过回调输出
        rec_player_config_t *player = &stream->config.player;

        if (player->input_source.enabled && player->input_source.read_cb) {
            uint16_t samples_read = player->input_source.read_cb(stream->data_buf,
                                                               stream->frame_size,
                                                               player->input_source.user_data);
            if (samples_read > 0) {
                // 通过输出回调传递数据（替代直接输出接口）
                if (player->output_cb) {
                    player->output_cb(stream->data_buf, samples_read * REC_CHANNELS, player->output_user_data);
                }

                stream->total_samples += samples_read;
                stream->data_ready = 1;

                // 调用数据就绪回调
                if (stream->on_data_ready) {
                    stream->on_data_ready(stream, stream->data_buf, samples_read);
                }
            }
        }
    }

    return REC_ERR_OK;
}

// --- 流停止实现 ---
static uint8_t rec_stream_stop_internal(rec_stream_t *stream) {
    LOG_INFO("[STOP] rec_stream_stop called\n");

    if (!stream) {
        return rec_err(REC_CONFIG_NO_SOURCE_IN);
    }

    // 停止所有数据源
    if (stream->type == REC_TYPE_RECORDER) {
        rec_recorder_config_t *recorder = &stream->config.recorder;

        // 停止输入源
        for (uint8_t i = 0; i < recorder->input_source_count; i++) {
            rec_source_config_t *source = &recorder->input_sources[i];
            if (source->enabled && source->stop_cb) {
                source->stop_cb(stream, source->user_data);
                LOG_INFO("[STOP] Input source %d stopped\n", i);
            }
        }

        // 停止输出源
        if (recorder->output_source.enabled && recorder->output_source.stop_cb) {
            recorder->output_source.stop_cb(stream, recorder->output_source.user_data);
            LOG_INFO("[STOP] Output source stopped\n");
        }

    } else if (stream->type == REC_TYPE_PLAYER) {
        rec_player_config_t *player = &stream->config.player;

        // 停止输入源
        if (player->input_source.enabled && player->input_source.stop_cb) {
            player->input_source.stop_cb(stream, player->input_source.user_data);
            LOG_INFO("[STOP] Player input source stopped\n");
        }
    }

    stream->state = REC_STREAM_STOPPED;
    LOG_INFO("[STOP] Stream stopped\n");

    return REC_ERR_OK;
}

// --- 其他操作实现 ---
static uint8_t rec_stream_pause_internal(rec_stream_t *stream) {
    if (!stream) return rec_err(REC_CONFIG_NO_SOURCE_IN);
    stream->state = REC_STREAM_PAUSED;
    LOG_INFO("[PAUSE] Stream paused\n");
    return REC_ERR_OK;
}

static uint8_t rec_stream_resume_internal(rec_stream_t *stream) {
    if (!stream) return rec_err(REC_CONFIG_NO_SOURCE_IN);
    stream->state = REC_STREAM_RUNNING;
    LOG_INFO("[RESUME] Stream resumed\n");
    return REC_ERR_OK;
}

static void rec_stream_update_internal(rec_stream_t *stream) {
    if (!stream) return;

    stream->current_pos_ms += REC_FRAME_SIZE_MS;
    stream->data_request = 1;

    // 自动停止检查
    if (stream->file_config.auto_stop_ms > 0 &&
        stream->current_pos_ms >= stream->file_config.auto_stop_ms) {
        stream->state = REC_STREAM_STOPPED;
    }
}

// --- 对外接口包装 ---
uint8_t rec_stream_start(rec_stream_t *stream, rec_file_config_t *file) {
    rec_opt_t *opt = (rec_opt_t *)stream->opt;
    return opt->start(stream, file);
}

uint8_t rec_stream_process(rec_stream_t *stream) {
    rec_opt_t *opt = (rec_opt_t *)stream->opt;
    return opt->process(stream);
}

uint8_t rec_stream_stop(rec_stream_t *stream) {
    rec_opt_t *opt = (rec_opt_t *)stream->opt;
    return opt->stop(stream);
}

uint8_t rec_stream_pause(rec_stream_t *stream) {
    rec_opt_t *opt = (rec_opt_t *)stream->opt;
    return opt->pause(stream);
}

uint8_t rec_stream_resume(rec_stream_t *stream) {
    rec_opt_t *opt = (rec_opt_t *)stream->opt;
    return opt->resume(stream);
}

void rec_stream_update(rec_stream_t *stream) {
    rec_opt_t *opt = (rec_opt_t *)stream->opt;
    opt->update(stream);
}

uint8_t rec_stream_data_ready(rec_stream_t *stream) {
    return stream ? stream->data_ready : 0;
}

uint16_t rec_stream_get_play_data(rec_stream_t *stream, int16_t *buf, uint16_t len) {
    if (!stream || !buf || stream->type != REC_TYPE_PLAYER) {
        return 0;
    }

    uint16_t samples_to_copy = (len > stream->frame_size) ? stream->frame_size : len;
    memcpy(buf, stream->data_buf, samples_to_copy * REC_CHANNELS * sizeof(int16_t));

    return samples_to_copy;
}

void rec_stream_set_data_ready_cb(rec_stream_t *stream, rec_data_ready_cb_t cb, void *user_data) {
    if (stream) {
        stream->on_data_ready = cb;
        stream->on_data_ready_user = user_data;
    }
}

uint8_t rec_recorder_add_source(rec_stream_t *stream, rec_source_config_t *source) {
    if (!stream || !source || stream->type != REC_TYPE_RECORDER) {
        return rec_err(REC_CONFIG_NO_SOURCE_IN);
    }

    rec_recorder_config_t *recorder = &stream->config.recorder;

    if (recorder->input_source_count >= REC_INPUT_SOURCE_MAX) {
        return rec_err(REC_ERR_UNSUPPORTED);
    }

    recorder->input_sources[recorder->input_source_count] = *source;
    recorder->input_source_count++;

    LOG_INFO("[ADD_SOURCE] Added input source, total: %d\n", recorder->input_source_count);

    return REC_ERR_OK;
}

void rec_recorder_set_mix_mode(rec_stream_t *stream, rec_mix_mode_t mode) {
    if (stream && stream->type == REC_TYPE_RECORDER) {
        stream->config.recorder.mix_mode = mode;
        LOG_INFO("[MIX_MODE] Set mix mode: %d\n", mode);
    }
}
