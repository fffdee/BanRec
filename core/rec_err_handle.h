#ifndef REC_ERR_HANDLE_H
#define REC_ERR_HANDLE_H

#include <stdint.h>
#include <stddef.h>
#include "rec_config.h"
// 错误类型枚举
typedef enum {
    REC_ERR_OK = 0,           // 成功
    REC_INIT_FAIL,
    REC_CONFIG_NO_SOURCE_IN, // 配置无输入源
    REC_CONFIG_NO_SOURCE_OUT,// 配置无输出源
    REC_ERR_RECORD_TIMEOUT,    // 录制超时
    REC_ERR_UNSUPPORTED,      // 不支持的操作
    REC_ERR_STATE,            // 状态错误
    REC_CONFIG_NO_OPT,           // 未设置opt指针
    REC_CONFIG_NO_OPT_FUNC,      // opt结构体中函数指针为NULL
    REC_ERR_MAX = 0xFF,
    // ...可扩展其他错误类型...
} rec_err_t;

// 错误信息结构体
typedef struct {
    uint8_t err_code;
    const char *file;
    int line;
} rec_err_info_t;



// 使能/关闭错误打印
void rec_err_enable(uint8_t en);

// 打印当前队列所有错误
void rec_err_print_queue(void);

// 对外唯一错误上报接口，返回错误码
uint8_t rec_err(uint8_t err_code);

// 对外调试打印接口，直接printf
void rec_debug_printf(const char *fmt, ...);

// 运行时可控LOG_INFO宏（声明在头文件中供其他模块使用）
void rec_log_enable(uint8_t en);

// LOG_INFO宏定义（需要访问内部全局变量，通过函数包装）
#ifdef __cplusplus
extern "C" {
#endif

#define LOG_INFO(fmt, ...) rec_debug_printf("[INFO] " fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // REC_ERR_HANDLE_H
