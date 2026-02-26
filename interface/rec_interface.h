#ifndef REC_INTERFACE_H
#define REC_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "rec_config.h"
#include "rec_core.h"


/*============================================================================
 * 接口层 - 对接HAL层，提供回调实现
 * 将HAL层的具体硬件操作适配为Core层需要的回调函数
 *============================================================================*/

/**
 * @brief 初始化接口层
 * @return 0成功，其他失败
 */
uint8_t rec_interface_init(void);

/**
 * @brief 去初始化接口层
 * @return 0成功，其他失败
 */
uint8_t rec_interface_deinit(void);

/**
 * @brief 创建IIS输入源（录音用）
 * @param volume 音量（0-100）
 * @return 数据源配置
 */
rec_source_config_t rec_interface_create_iis_input(uint8_t volume);

/**
 * @brief 创建IIS输出源（播放用）
 * @param volume 音量（0-100）
 * @return 数据源配置
 */
rec_source_config_t rec_interface_create_iis_output(uint8_t volume);

/**
 * @brief 创建EMMC录音输出源
 * @param filename 文件名（NULL使用默认）
 * @return 数据源配置
 */
rec_source_config_t rec_interface_create_emmc_record_output(const char *filename);

/**
 * @brief 创建EMMC播放输入源
 * @param filename 文件名
 * @return 数据源配置
 */
rec_source_config_t rec_interface_create_emmc_play_input(const char *filename);

/**
 * @brief 创建自定义回调数据源
 * @param read_cb 读取回调
 * @param write_cb 写入回调
 * @param start_cb 启动回调
 * @param stop_cb 停止回调
 * @param user_data 用户数据
 * @param volume 音量（0-100）
 * @return 数据源配置
 */
rec_source_config_t rec_interface_create_custom_source(
    rec_data_read_cb_t read_cb,
    rec_data_write_cb_t write_cb,
    rec_iface_start_cb_t start_cb,
    rec_iface_stop_cb_t stop_cb,
    void *user_data,
    uint8_t volume
);

/**
 * @brief 从节点表创建数据源配置
 * @param node_type 节点类型
 * @param volume 音量（0-100）
 * @param user_data 用户数据（可选，用于文件名等）
 * @return 数据源配置，失败时type为REC_SOURCE_NONE
 */
rec_source_config_t rec_interface_create_source_from_node(
    rec_iface_type_t node_type,
    uint8_t volume,
    void *user_data
);

/**
 * @brief 获取接口节点表
 * @return 节点表指针
 */
rec_node_t *rec_interface_get_node_table(void);

#ifdef __cplusplus
}
#endif

#endif // REC_INTERFACE_H
