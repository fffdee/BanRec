#ifndef REC_CORE_H
#define REC_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "rec_config.h"

/*============================================================================
 * 前向声明
 *============================================================================*/
typedef struct rec_stream rec_stream_t;

/*============================================================================
 * 回调函数类型定义 (实现完全解耦)
 *============================================================================*/

/**
 * @brief 数据读取回调 - 从数据源读取数据
 * @param buf 输出缓冲区
 * @param len 请求的样本数
 * @param user_data 用户自定义数据
 * @return 实际读取的样本数
 */
typedef uint16_t (*rec_data_read_cb_t)(int16_t *buf, uint16_t len, void *user_data);

/**
 * @brief 数据写入回调 - 向数据源写入数据
 * @param buf 输入缓冲区
 * @param len 样本数
 * @param user_data 用户自定义数据
 * @return 0成功，其他失败
 */
typedef uint8_t (*rec_data_write_cb_t)(const int16_t *buf, uint16_t len, void *user_data);

/**
 * @brief 数据输出回调 - 播放时的数据输出（替代直接输出接口）
 * @param buf 输出缓冲区
 * @param len 样本数
 * @param user_data 用户自定义数据
 * @return 0成功，其他失败
 */
typedef uint8_t (*rec_data_output_cb_t)(const int16_t *buf, uint16_t len, void *user_data);

/**
 * @brief 数据就绪回调 - 当有新数据可用时调用
 * @param stream 流对象
 * @param buf 数据缓冲区
 * @param len 样本数
 */
typedef void (*rec_data_ready_cb_t)(rec_stream_t *stream, int16_t *buf, uint16_t len);

/**
 * @brief 接口启动/停止回调
 * @param stream 流对象
 * @param user_data 用户自定义数据
 * @return 0成功，其他失败
 */
typedef uint8_t (*rec_iface_start_cb_t)(rec_stream_t *stream, void *user_data);
typedef uint8_t (*rec_iface_stop_cb_t)(rec_stream_t *stream, void *user_data);

/*============================================================================
 * 数据源配置结构体
 *============================================================================*/
typedef struct {
    rec_source_type_t   type;           // 数据源类型
    rec_data_read_cb_t  read_cb;        // 读取回调
    rec_data_write_cb_t write_cb;       // 写入回调
    rec_iface_start_cb_t start_cb;      // 启动回调
    rec_iface_stop_cb_t  stop_cb;       // 停止回调
    void               *user_data;      // 用户自定义数据
    uint8_t             enabled;        // 是否启用
    int16_t             volume;         // 音量（0-100，用于混音）
} rec_source_config_t;

/*============================================================================
 * 录制配置结构体
 *============================================================================*/
typedef struct {
    // 输入源配置（支持多数据源混音）
    rec_source_config_t  input_sources[REC_INPUT_SOURCE_MAX];
    uint8_t              input_source_count;    // 实际输入源数量
    rec_mix_mode_t       mix_mode;              // 混音模式
    
    // 输出源配置（录制存储目标）
    rec_source_config_t  output_source;         // 输出目标（如EMMC）
    
} rec_recorder_config_t;

/*============================================================================
 * 播放配置结构体
 *============================================================================*/
typedef struct {
    // 输入源配置（播放数据来源，如EMMC文件）
    rec_source_config_t  input_source;
    
    // 数据输出回调（替代直接输出接口，便于混音）
    rec_data_output_cb_t output_cb;             // 数据输出回调
    void                *output_user_data;      // 输出回调用户数据
    
} rec_player_config_t;

/*============================================================================
 * 文件配置结构体
 *============================================================================*/
typedef struct {
    char        *path;              // 路径
    char        *file_name;         // 文件名
    uint8_t      channels;          // 声道数
    uint8_t      sample_width;      // 采样宽度（位）
    uint32_t     sample_rate;       // 采样率
    uint32_t     auto_stop_ms;      // 自动停止时间（0=不自动停止）
} rec_file_config_t;

/*============================================================================
 * 流对象结构体
 *============================================================================*/
struct rec_stream {
    rec_stream_type_t    type;              // 流类型
    rec_stream_state_t   state;             // 流状态
    
    // 配置联合体
    union {
        rec_recorder_config_t recorder;     // 录制配置
        rec_player_config_t   player;       // 播放配置
    } config;
    
    // 文件配置
    rec_file_config_t    file_config;
    
    // 数据缓冲区
    int16_t              data_buf[REC_FRAME_SAMPLES * REC_CHANNELS];
    int16_t              mix_buf[REC_FRAME_SAMPLES * REC_CHANNELS];  // 混音临时缓冲
    uint16_t             frame_size;        // 当前帧大小（样本数）
    
    // 状态标志
    uint8_t              data_ready;        // 数据就绪标志
    uint8_t              data_request;      // 数据请求标志
    uint32_t             current_pos_ms;    // 当前位置（毫秒）
    uint32_t             total_samples;     // 总采样数
    
    // 数据就绪回调（外部获取数据用）
    rec_data_ready_cb_t  on_data_ready;
    void                *on_data_ready_user;
    
    // 操作函数表指针
    void                *opt;
};

/*============================================================================
 * 操作函数表
 *============================================================================*/
typedef struct {
    uint8_t (*start)(rec_stream_t *stream, rec_file_config_t *file);
    uint8_t (*process)(rec_stream_t *stream);
    uint8_t (*stop)(rec_stream_t *stream);
    uint8_t (*pause)(rec_stream_t *stream);
    uint8_t (*resume)(rec_stream_t *stream);
    void    (*update)(rec_stream_t *stream);
} rec_opt_t;

/*============================================================================
 * 对外接口
 *============================================================================*/

/**
 * @brief 初始化录制流
 * @param stream 流对象指针
 * @param config 录制配置
 * @return 0成功，其他失败
 */
uint8_t rec_recorder_init(rec_stream_t *stream, rec_recorder_config_t *config);

/**
 * @brief 初始化播放流
 * @param stream 流对象指针
 * @param config 播放配置
 * @return 0成功，其他失败
 */
uint8_t rec_player_init(rec_stream_t *stream, rec_player_config_t *config);

/**
 * @brief 启动流
 * @param stream 流对象指针
 * @param file 文件配置（可为NULL使用默认）
 * @return 0成功，其他失败
 */
uint8_t rec_stream_start(rec_stream_t *stream, rec_file_config_t *file);

/**
 * @brief 处理流数据（周期性调用）
 * @param stream 流对象指针
 * @return 0成功，其他失败
 */
uint8_t rec_stream_process(rec_stream_t *stream);

/**
 * @brief 停止流
 * @param stream 流对象指针
 * @return 0成功，其他失败
 */
uint8_t rec_stream_stop(rec_stream_t *stream);

/**
 * @brief 暂停流
 * @param stream 流对象指针
 * @return 0成功，其他失败
 */
uint8_t rec_stream_pause(rec_stream_t *stream);

/**
 * @brief 恢复流
 * @param stream 流对象指针
 * @return 0成功，其他失败
 */
uint8_t rec_stream_resume(rec_stream_t *stream);

/**
 * @brief 定时更新（在定时器/中断中调用）
 * @param stream 流对象指针
 */
void rec_stream_update(rec_stream_t *stream);

/**
 * @brief 检查数据是否就绪
 * @param stream 流对象指针
 * @return 1就绪，0未就绪
 */
uint8_t rec_stream_data_ready(rec_stream_t *stream);

/**
 * @brief 获取播放数据（通过回调方式）
 * @param stream 流对象指针
 * @param buf 输出缓冲区
 * @param len 请求样本数
 * @return 实际获取的样本数
 */
uint16_t rec_stream_get_play_data(rec_stream_t *stream, int16_t *buf, uint16_t len);

/**
 * @brief 设置数据就绪回调
 * @param stream 流对象指针
 * @param cb 回调函数
 * @param user_data 用户数据
 */
void rec_stream_set_data_ready_cb(rec_stream_t *stream, rec_data_ready_cb_t cb, void *user_data);

/**
 * @brief 添加录制输入源
 * @param stream 流对象指针
 * @param source 数据源配置
 * @return 0成功，其他失败
 */
uint8_t rec_recorder_add_source(rec_stream_t *stream, rec_source_config_t *source);

/**
 * @brief 设置混音模式
 * @param stream 流对象指针
 * @param mode 混音模式
 */
void rec_recorder_set_mix_mode(rec_stream_t *stream, rec_mix_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif // REC_CORE_H
