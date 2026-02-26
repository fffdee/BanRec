#ifndef REC_API_H
#define REC_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "rec_core.h"

/*============================================================================
 * API层 - 提供简洁易用的对外接口
 *============================================================================*/

/**
 * @brief 初始化录制模块
 * @return 0成功，其他失败
 */
uint8_t rec_api_init(void);

/**
 * @brief 去初始化录制模块
 * @return 0成功，其他失败
 */
uint8_t rec_api_deinit(void);

/*============================================================================
 * 录制器API
 *============================================================================*/

/**
 * @brief 配置录制器（添加输入源）
 * @param source 数据源配置
 * @return 0成功，其他失败
 */
uint8_t rec_api_recorder_add_source(rec_source_config_t *source);

/**
 * @brief 设置录制输出目标（EMMC文件）
 * @param filename 文件名（NULL使用默认）
 * @return 0成功，其他失败
 */
uint8_t rec_api_recorder_set_output(const char *filename);

/**
 * @brief 设置录制输出源（从节点表）
 * @param node_type 输出节点类型
 * @param volume 音量（0-100）
 * @param user_data 用户数据（如文件名）
 * @return 0成功，其他失败
 */
uint8_t rec_api_recorder_set_output_from_node(
    rec_iface_type_t node_type,
    uint8_t volume,
    void *user_data
);

/**
 * @brief 设置混音模式
 * @param mode 混音模式
 */
void rec_api_recorder_set_mix_mode(rec_mix_mode_t mode);

/**
 * @brief 启动录制
 * @return 0成功，其他失败
 */
uint8_t rec_api_recorder_start(void);

/**
 * @brief 停止录制
 * @return 0成功，其他失败
 */
uint8_t rec_api_recorder_stop(void);

/**
 * @brief 暂停录制
 * @return 0成功，其他失败
 */
uint8_t rec_api_recorder_pause(void);

/**
 * @brief 恢复录制
 * @return 0成功，其他失败
 */
uint8_t rec_api_recorder_resume(void);

/**
 * @brief 处理录制数据（周期性调用）
 * @return 0成功，其他失败
 */
uint8_t rec_api_recorder_process(void);

/*============================================================================
 * 播放器API
 *============================================================================*/

/**
 * @brief 配置播放器输入源（EMMC文件）
 * @param filename 文件名
 * @return 0成功，其他失败
 */
uint8_t rec_api_player_set_input(const char *filename);

/**
 * @brief 设置播放输出回调
 * @param output_cb 输出回调函数
 * @param user_data 用户数据
 */
void rec_api_player_set_output_callback(rec_data_output_cb_t output_cb, void *user_data);

/**
 * @brief 启动播放
 * @return 0成功，其他失败
 */
uint8_t rec_api_player_start(void);

/**
 * @brief 停止播放
 * @return 0成功，其他失败
 */
uint8_t rec_api_player_stop(void);

/**
 * @brief 暂停播放
 * @return 0成功，其他失败
 */
uint8_t rec_api_player_pause(void);

/**
 * @brief 恢复播放
 * @return 0成功，其他失败
 */
uint8_t rec_api_player_resume(void);

/**
 * @brief 处理播放数据（周期性调用）
 * @return 0成功，其他失败
 */
uint8_t rec_api_player_process(void);

/**
 * @brief 获取播放数据（可选，用于外部混音）
 * @param buf 输出缓冲区
 * @param len 请求样本数
 * @return 实际获取的样本数
 */
uint16_t rec_api_player_get_data(int16_t *buf, uint16_t len);

/*============================================================================
 * 定时器更新API（在定时器中断中调用）
 *============================================================================*/

/**
 * @brief 更新录制器状态（定时器中调用）
 */
void rec_api_recorder_update(void);

/**
 * @brief 更新播放器状态（定时器中调用）
 */
void rec_api_player_update(void);

#ifdef __cplusplus
}
#endif

#endif // REC_API_H
