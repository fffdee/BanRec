// rec_interface.c
// 接口层实现 - 对接HAL层，提供具体回调实现
// 将HAL层的硬件操作适配为Core层需要的回调接口

#include "rec_interface.h"
#include "rec_err_handle.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*============================================================================
 * EMMC文件管理
 *============================================================================*/

// EMMC文件上下文结构体
typedef struct {
    FILE *fp;
    char filename[256];
    uint8_t is_recording;  // 1:录音，0:播放
    uint32_t total_samples;
} emmc_file_context_t;

// --- WAV文件头结构 ---
typedef struct {
    char riff[4];              // "RIFF"
    uint32_t chunk_size;       // 文件大小 - 8
    char wave[4];              // "WAVE"
    char fmt[4];               // "fmt "
    uint32_t subchunk1_size;   // 16
    uint16_t audio_format;     // 1 (PCM)
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];              // "data"
    uint32_t subchunk2_size;   // 数据大小
} wav_header_t;

// --- WAV头写入函数 ---
static void write_wav_header(FILE *fp, uint32_t data_len, uint32_t sample_rate,
                            uint16_t channels, uint16_t bits_per_sample) {
    if (!fp) return;

    wav_header_t header = {0};
    memcpy(header.riff, "RIFF", 4);
    header.chunk_size = 36 + data_len;
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    header.subchunk1_size = 16;
    header.audio_format = 1;
    header.channels = channels;
    header.sample_rate = sample_rate;
    header.byte_rate = sample_rate * channels * bits_per_sample / 8;
    header.block_align = channels * bits_per_sample / 8;
    header.bits_per_sample = bits_per_sample;
    memcpy(header.data, "data", 4);
    header.subchunk2_size = data_len;

    fseek(fp, 0, SEEK_SET);
    fwrite(&header, sizeof(wav_header_t), 1, fp);

    LOG_INFO("[WAV_HEADER] Written: samples=%u, rate=%u, ch=%u, bits=%u\n",
             data_len / (channels * bits_per_sample / 8), sample_rate, channels, bits_per_sample);
}

// --- 跳过WAV文件头 ---
static uint8_t skip_wav_header(FILE *fp) {
    if (!fp) return rec_err(REC_CONFIG_NO_SOURCE_IN);

    wav_header_t header;
    fseek(fp, 0, SEEK_SET);
    size_t read_size = fread(&header, 1, sizeof(wav_header_t), fp);

    if (read_size == sizeof(wav_header_t) &&
        memcmp(header.riff, "RIFF", 4) == 0 &&
        memcmp(header.wave, "WAVE", 4) == 0) {
        LOG_INFO("[WAV_HEADER] Valid WAV file detected, skipped header\n");
        return REC_ERR_OK;
    }

    // 不是WAV文件，回到开头
    fseek(fp, 0, SEEK_SET);
    LOG_INFO("[WAV_HEADER] Not a WAV file, reading from start\n");
    return REC_ERR_OK;
}

/*============================================================================
 * EMMC接口回调实现
 *============================================================================*/

// EMMC录音启动回调
static uint8_t emmc_record_start_cb(rec_stream_t *stream, void *user_data) {
    if (!stream) return rec_err(REC_CONFIG_NO_SOURCE_IN);

    emmc_file_context_t *ctx = (emmc_file_context_t *)malloc(sizeof(emmc_file_context_t));
    if (!ctx) return rec_err(REC_INIT_FAIL);

    memset(ctx, 0, sizeof(emmc_file_context_t));

    // 构建文件路径
    const char *filename = stream->file_config.file_name;
    snprintf(ctx->filename, sizeof(ctx->filename), "%s%s",
             stream->file_config.path, filename);

    // 打开文件（写入模式，预留44字节WAV头）
    ctx->fp = fopen(ctx->filename, "wb+");
    if (!ctx->fp) {
        LOG_INFO("[EMMC_RECORD] Failed to open file: %s\n", ctx->filename);
        free(ctx);
        return rec_err(REC_ERR_UNSUPPORTED);
    }

    // 预写空的WAV头
    wav_header_t header = {0};
    fwrite(&header, sizeof(wav_header_t), 1, ctx->fp);

    ctx->is_recording = 1;
    ctx->total_samples = 0;

    // 保存上下文到output_source的user_data
    stream->config.recorder.output_source.user_data = ctx;

    LOG_INFO("[EMMC_RECORD] File opened: %s\n", ctx->filename);

    return REC_ERR_OK;
}

// EMMC录音停止回调
static uint8_t emmc_record_stop_cb(rec_stream_t *stream, void *user_data) {
    if (!stream) return rec_err(REC_CONFIG_NO_SOURCE_IN);

    emmc_file_context_t *ctx = (emmc_file_context_t *)stream->config.recorder.output_source.user_data;
    if (!ctx) return REC_ERR_OK;

    if (ctx->fp) {
        // 计算数据大小
        uint32_t data_size = ctx->total_samples * stream->file_config.channels *
                            (stream->file_config.sample_width / 8);

        // 补写WAV头
        write_wav_header(ctx->fp, data_size, stream->file_config.sample_rate,
                        stream->file_config.channels, stream->file_config.sample_width);

        fclose(ctx->fp);
        LOG_INFO("[EMMC_RECORD] File closed: %s, total samples: %u\n",
                 ctx->filename, ctx->total_samples);
    }

    free(ctx);
    stream->config.recorder.output_source.user_data = NULL;

    return REC_ERR_OK;
}

// EMMC录音写入回调
static uint8_t emmc_record_write_cb(const int16_t *buf, uint16_t len, void *user_data) {
    emmc_file_context_t *ctx = (emmc_file_context_t *)user_data;

    if (!ctx || !ctx->fp || !buf || len == 0) {
        return rec_err(REC_CONFIG_NO_SOURCE_IN);
    }

    size_t written = fwrite(buf, sizeof(int16_t), len, ctx->fp);
    if (written == len) {
        ctx->total_samples += len / REC_CHANNELS;
        return REC_ERR_OK;
    }

    LOG_INFO("[EMMC_RECORD] Write failed: expected %u, written %zu\n", len, written);
    return rec_err(REC_ERR_STATE);
}

// EMMC播放启动回调
static uint8_t emmc_play_start_cb(rec_stream_t *stream, void *user_data) {
    if (!stream) return rec_err(REC_CONFIG_NO_SOURCE_IN);

    emmc_file_context_t *ctx = (emmc_file_context_t *)malloc(sizeof(emmc_file_context_t));
    if (!ctx) return rec_err(REC_INIT_FAIL);

    memset(ctx, 0, sizeof(emmc_file_context_t));

    // 构建文件路径
    const char *filename = stream->file_config.file_name;
    snprintf(ctx->filename, sizeof(ctx->filename), "%s%s",
             stream->file_config.path, filename);

    // 打开文件（读取模式）
    ctx->fp = fopen(ctx->filename, "rb");
    if (!ctx->fp) {
        LOG_INFO("[EMMC_PLAY] Failed to open file: %s\n", ctx->filename);
        free(ctx);
        return rec_err(REC_ERR_UNSUPPORTED);
    }

    // 跳过WAV头
    skip_wav_header(ctx->fp);

    ctx->is_recording = 0;
    ctx->total_samples = 0;

    // 保存上下文到input_source的user_data
    stream->config.player.input_source.user_data = ctx;

    LOG_INFO("[EMMC_PLAY] File opened: %s\n", ctx->filename);

    return REC_ERR_OK;
}

// EMMC播放停止回调
static uint8_t emmc_play_stop_cb(rec_stream_t *stream, void *user_data) {
    if (!stream) return rec_err(REC_CONFIG_NO_SOURCE_IN);

    emmc_file_context_t *ctx = (emmc_file_context_t *)stream->config.player.input_source.user_data;
    if (!ctx) return REC_ERR_OK;

    if (ctx->fp) {
        fclose(ctx->fp);
        LOG_INFO("[EMMC_PLAY] File closed: %s, total samples: %u\n",
                 ctx->filename, ctx->total_samples);
    }

    free(ctx);
    stream->config.player.input_source.user_data = NULL;

    return REC_ERR_OK;
}

// EMMC播放读取回调
static uint16_t emmc_play_read_cb(int16_t *buf, uint16_t samples, void *user_data) {
    emmc_file_context_t *ctx = (emmc_file_context_t *)user_data;

    if (!ctx || !ctx->fp || !buf || samples == 0) {
        return 0;
    }

    uint16_t total_values = samples * REC_CHANNELS;
    size_t read_count = fread(buf, sizeof(int16_t), total_values, ctx->fp);

    if (read_count > 0) {
        ctx->total_samples += read_count / REC_CHANNELS;
        return read_count / REC_CHANNELS;  // 返回读取的样本数（帧数）
    }

    if (feof(ctx->fp)) {
        LOG_INFO("[EMMC_PLAY] End of file reached\n");
    }

    return 0;
}

/*============================================================================
 * IIS接口回调实现（模拟）
 *============================================================================*/

// IIS输入启动回调（录音）
static uint8_t iis_input_start_cb(rec_stream_t *stream, void *user_data) {
    LOG_INFO("[IIS_INPUT] Started\n");
    // 实际硬件初始化在这里进行
    return REC_ERR_OK;
}

// IIS输入停止回调
static uint8_t iis_input_stop_cb(rec_stream_t *stream, void *user_data) {
    LOG_INFO("[IIS_INPUT] Stopped\n");
    return REC_ERR_OK;
}

// IIS输入读取回调（从硬件读取数据）
static uint16_t iis_input_read_cb(int16_t *buf, uint16_t samples, void *user_data) {
    if (!buf || samples == 0) return 0;

    // 模拟从IIS硬件读取数据
    // 实际应用中，这里应该从DMA缓冲区或硬件FIFO读取
    for (uint16_t i = 0; i < samples * REC_CHANNELS; i++) {
        buf[i] = 0;  // 填充静音数据（实际应从硬件读取）
    }

    return samples;
}

// IIS输出启动回调（播放）
static uint8_t iis_output_start_cb(rec_stream_t *stream, void *user_data) {
    LOG_INFO("[IIS_OUTPUT] Started\n");
    return REC_ERR_OK;
}

// IIS输出停止回调
static uint8_t iis_output_stop_cb(rec_stream_t *stream, void *user_data) {
    LOG_INFO("[IIS_OUTPUT] Stopped\n");
    return REC_ERR_OK;
}

// IIS输出写入回调（向硬件写入数据）
static uint8_t iis_output_write_cb(const int16_t *buf, uint16_t len, void *user_data) {
    if (!buf || len == 0) return rec_err(REC_CONFIG_NO_SOURCE_IN);

    // 模拟向IIS硬件写入数据
    // 实际应用中，这里应该写入DMA缓冲区或硬件FIFO
    LOG_INFO("[IIS_OUTPUT] Write %u samples\n", len / REC_CHANNELS);

    return REC_ERR_OK;
}

/*============================================================================
 * 接口节点表定义
 *============================================================================*/

// --- 接口节点表 ---
// 每个节点包含类型、数据处理函数、开始/停止句柄
static rec_node_t rec_interface_node_table[REC_NODES_MAX] = {
    // IIS输入节点
    {
        .type = REC_IFACE_IIS_IN,
        .read_cb = iis_input_read_cb,
        .write_cb = NULL,
        .start_cb = iis_input_start_cb,
        .stop_cb = iis_input_stop_cb,
        .user_data = NULL
    },
    
    // IIS输出节点
    {
        .type = REC_IFACE_IIS_OUT,
        .read_cb = NULL,
        .write_cb = iis_output_write_cb,
        .start_cb = iis_output_start_cb,
        .stop_cb = iis_output_stop_cb,
        .user_data = NULL
    },
    
    // EMMC输入节点（文件播放）
    {
        .type = REC_IFACE_EMMC_IN,
        .read_cb = emmc_play_read_cb,
        .write_cb = NULL,
        .start_cb = emmc_play_start_cb,
        .stop_cb = emmc_play_stop_cb,
        .user_data = NULL
    },
    
    // EMMC输出节点（文件录制）
    {
        .type = REC_IFACE_EMMC_OUT,
        .read_cb = NULL,
        .write_cb = emmc_record_write_cb,
        .start_cb = emmc_record_start_cb,
        .stop_cb = emmc_record_stop_cb,
        .user_data = NULL
    },
    
    // NOR Flash输入节点（占位）
    {
        .type = REC_IFACE_NOR_FLASH_IN,
        .read_cb = NULL,  // TODO: 实现NOR Flash读取
        .write_cb = NULL,
        .start_cb = NULL,
        .stop_cb = NULL,
        .user_data = NULL
    },
    
    // NOR Flash输出节点（占位）
    {
        .type = REC_IFACE_NOR_FLASH_OUT,
        .read_cb = NULL,
        .write_cb = NULL,  // TODO: 实现NOR Flash写入
        .start_cb = NULL,
        .stop_cb = NULL,
        .user_data = NULL
    }
};

/*============================================================================
 * 对外接口实现
 *============================================================================*/

uint8_t rec_interface_init(void) {
    LOG_INFO("[INTERFACE] Interface layer initialized\n");
    return REC_ERR_OK;
}

uint8_t rec_interface_deinit(void) {
    LOG_INFO("[INTERFACE] Interface layer deinitialized\n");
    return REC_ERR_OK;
}

rec_source_config_t rec_interface_create_iis_input(uint8_t volume) {
    rec_source_config_t source = {0};

    source.type = REC_SOURCE_IIS;
    source.read_cb = iis_input_read_cb;
    source.write_cb = NULL;
    source.start_cb = iis_input_start_cb;
    source.stop_cb = iis_input_stop_cb;
    source.user_data = NULL;
    source.enabled = 1;
    source.volume = volume;

    return source;
}

rec_source_config_t rec_interface_create_iis_output(uint8_t volume) {
    rec_source_config_t source = {0};

    source.type = REC_SOURCE_IIS;
    source.read_cb = NULL;
    source.write_cb = iis_output_write_cb;
    source.start_cb = iis_output_start_cb;
    source.stop_cb = iis_output_stop_cb;
    source.user_data = NULL;
    source.enabled = 1;
    source.volume = volume;

    return source;
}

rec_source_config_t rec_interface_create_emmc_record_output(const char *filename) {
    rec_source_config_t source = {0};

    source.type = REC_SOURCE_EMMC;
    source.read_cb = NULL;
    source.write_cb = emmc_record_write_cb;
    source.start_cb = emmc_record_start_cb;
    source.stop_cb = emmc_record_stop_cb;
    source.user_data = NULL;  // 在start_cb中分配
    source.enabled = 1;
    source.volume = 100;

    return source;
}

rec_source_config_t rec_interface_create_emmc_play_input(const char *filename) {
    rec_source_config_t source = {0};

    source.type = REC_SOURCE_EMMC;
    source.read_cb = emmc_play_read_cb;
    source.write_cb = NULL;
    source.start_cb = emmc_play_start_cb;
    source.stop_cb = emmc_play_stop_cb;
    source.user_data = NULL;  // 在start_cb中分配
    source.enabled = 1;
    source.volume = 100;

    return source;
}

rec_source_config_t rec_interface_create_custom_source(
    rec_data_read_cb_t read_cb,
    rec_data_write_cb_t write_cb,
    rec_iface_start_cb_t start_cb,
    rec_iface_stop_cb_t stop_cb,
    void *user_data,
    uint8_t volume
) {
    rec_source_config_t source = {0};

    source.type = REC_SOURCE_CALLBACK;
    source.read_cb = read_cb;
    source.write_cb = write_cb;
    source.start_cb = start_cb;
    source.stop_cb = stop_cb;
    source.user_data = user_data;
    source.enabled = 1;
    source.volume = volume;

    return source;
}

rec_source_config_t rec_interface_create_source_from_node(
    rec_iface_type_t node_type,
    uint8_t volume,
    void *user_data
) {
    rec_source_config_t source = {0};
    
    // 查找对应的节点
    for (int i = 0; i < REC_NODES_MAX; i++) {
        if (rec_interface_node_table[i].type == node_type) {
            rec_node_t *node = &rec_interface_node_table[i];
            
            // 根据节点类型设置数据源类型
            switch (node_type) {
                case REC_IFACE_IIS_IN:
                case REC_IFACE_IIS_OUT:
                    source.type = REC_SOURCE_IIS;
                    break;
                case REC_IFACE_EMMC_IN:
                case REC_IFACE_EMMC_OUT:
                    source.type = REC_SOURCE_EMMC;
                    break;
                case REC_IFACE_NOR_FLASH_IN:
                case REC_IFACE_NOR_FLASH_OUT:
                    source.type = REC_SOURCE_NOR_FLASH;
                    break;
                default:
                    source.type = REC_SOURCE_NONE;
                    return source;
            }
            
            source.read_cb = node->read_cb;
            source.write_cb = node->write_cb;
            source.start_cb = node->start_cb;
            source.stop_cb = node->stop_cb;
            source.user_data = user_data ? user_data : node->user_data;
            source.enabled = 1;
            source.volume = volume;
            
            LOG_INFO("[INTERFACE] Created source from node type: %d\n", node_type);
            return source;
        }
    }
    
    // 未找到对应节点
    LOG_INFO("[INTERFACE] Node type %d not found in table\n", node_type);
    source.type = REC_SOURCE_NONE;
    return source;
}

rec_node_t *rec_interface_get_node_table(void) {
    return rec_interface_node_table;
}