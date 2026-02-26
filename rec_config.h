#ifndef _REC_CONFIG_H
#define _REC_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 基础音频参数配置
 *============================================================================*/
#define REC_SAMPLE_RATE         48000
#define REC_SAMPLE_WIDTH        16
#define REC_CHANNELS            2
#define REC_DEFAULT_PATH        "./"

/*============================================================================
 * 数据源/通道配置
 *============================================================================*/
#define REC_INPUT_SOURCE_MAX    4       // 最大输入源数量（混音支持）
#define REC_OUTPUT_SOURCE_MAX   2       // 最大输出源数量
#define REC_NODES_MAX           8       // 接口节点最大数量

/*============================================================================
 * 缓冲区配置
 *============================================================================*/
#define REC_FRAME_SIZE_MS       1       // 帧大小（毫秒）
#define REC_FRAME_SAMPLES       ((REC_SAMPLE_RATE * REC_FRAME_SIZE_MS) / 1000)
#define REC_BUFFER_FRAMES       100     // 缓冲帧数

/*============================================================================
 * 错误队列配置
 *============================================================================*/
#define REC_ERR_QUEUE_MAX       16

/*============================================================================
 * 流状态枚举
 *============================================================================*/
typedef enum {
    REC_STREAM_UNINIT = 0,      // 未初始化
    REC_STREAM_READY,           // 已初始化/可启动
    REC_STREAM_START,           // 启动中
    REC_STREAM_RUNNING,         // 运行中
    REC_STREAM_PAUSED,          // 暂停
    REC_STREAM_RESUMING,        // 恢复中
    REC_STREAM_STOPPED,         // 结束/停止
    REC_STATE_MAX
} rec_stream_state_t;

/*============================================================================
 * 流类型枚举
 *============================================================================*/
typedef enum {
    REC_TYPE_RECORDER = 0,      // 录制器
    REC_TYPE_PLAYER,            // 播放器
    REC_TYPE_MAX
} rec_stream_type_t;

/*============================================================================
 * 数据源类型枚举 (抽象层，与具体硬件解耦)
 *============================================================================*/
typedef enum {
    REC_SOURCE_NONE = 0,        // 无数据源
    REC_SOURCE_IIS,             // IIS/I2S 音频接口
    REC_SOURCE_EMMC,            // EMMC 存储
    REC_SOURCE_NOR_FLASH,       // NOR Flash 存储
    REC_SOURCE_CALLBACK,        // 回调数据源（用户自定义）
    REC_SOURCE_MAX
} rec_source_type_t;

/*============================================================================
 * 接口节点类型枚举
 *============================================================================*/
typedef enum {
    REC_IFACE_IIS_IN = 0,       // IIS输入（录音）
    REC_IFACE_IIS_OUT,          // IIS输出（播放）
    REC_IFACE_EMMC_IN,          // EMMC输入（文件播放）
    REC_IFACE_EMMC_OUT,         // EMMC输出（文件录制）
    REC_IFACE_NOR_FLASH_IN,     // NOR Flash输入
    REC_IFACE_NOR_FLASH_OUT,    // NOR Flash输出
    REC_IFACE_MAX
} rec_iface_type_t;

/*============================================================================
 * 接口节点结构体
 *============================================================================*/
typedef struct {
    rec_iface_type_t type;              // 节点类型
    rec_data_read_cb_t read_cb;         // 读取回调
    rec_data_write_cb_t write_cb;       // 写入回调
    rec_iface_start_cb_t start_cb;      // 启动回调
    rec_iface_stop_cb_t stop_cb;        // 停止回调
    void *user_data;                    // 用户数据
} rec_node_t;
