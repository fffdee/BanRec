#include "rec_err_handle.h"
#include <stdio.h>
#include <stdarg.h>

// 错误管理模块接口
typedef struct {
    uint8_t enabled; // 使能标志
    uint8_t log_enabled; // 日志开关标志
    rec_err_info_t queue[REC_ERR_QUEUE_MAX]; // 错误队列（定长数组）
    size_t queue_head;
    size_t queue_tail;
} rec_err_mgr_t;

// 错误管理全局变量
static rec_err_mgr_t g_err_mgr = {.enabled = 1, .log_enabled = 1};  // 默认开启

// 对外调试打印接口，直接使用printf
void rec_debug_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

// 错误码与字符串映射表
static const char *rec_err_str_table[] = {
    "OK",                                 // REC_ERR_OK
    "Recorder/Player init failed",        // REC_INIT_FAIL
    "Config error: no input source",      // REC_CONFIG_NO_SOURCE_IN
    "Config error: no output source",     // REC_CONFIG_NO_SOURCE_OUT
    "Record timeout: data not received in time", // REC_ERR_RECORD_TIMEOUT
    "Operation not supported by current hardware or mode", // REC_ERR_UNSUPPORTED
    "State error: operation not allowed in current state", // REC_ERR_STATE
    "Stream option pointer (opt) is NULL, cannot dispatch operation", // REC_CONFIG_NO_OPT
    "Stream option function pointer is NULL, cannot dispatch operation", // REC_CONFIG_NO_OPT_FUNC
    // ...可扩展其他错误类型...
};

void rec_err_enable(uint8_t en) {
    g_err_mgr.enabled = en ? 1 : 0;
}

// 日志开关设置接口
void rec_log_enable(uint8_t en) {
    g_err_mgr.log_enabled = en ? 1 : 0;
}

// 错误队列入队操作（满时覆盖最老消息）
void rec_err_queue_push(rec_err_mgr_t *mgr, rec_err_info_t info) {
    if (!mgr) return;
    mgr->queue[mgr->queue_tail] = info;
    mgr->queue_tail = (mgr->queue_tail + 1) % REC_ERR_QUEUE_MAX;
    if (mgr->queue_tail == mgr->queue_head) { // 队列满，顶掉最老的
        mgr->queue_head = (mgr->queue_head + 1) % REC_ERR_QUEUE_MAX;
    }
}

// 上报错误到队列
void rec_err_report(uint8_t err_code, const char *file, int line) {
    rec_err_info_t info = {err_code, file, line};
    rec_err_queue_push(&g_err_mgr, info);
}



// 打印当前队列所有错误（不清空队列）
void rec_err_print_queue(void) {
    size_t idx = g_err_mgr.queue_head;
    while (idx != g_err_mgr.queue_tail) {
        rec_err_info_t *info = &g_err_mgr.queue[idx];
        int max = sizeof(rec_err_str_table) / sizeof(rec_err_str_table[0]);
        if (g_err_mgr.enabled) {
            if (info->err_code < max) {
                printf("[REC_ERR] %s\n", rec_err_str_table[info->err_code]);
            } else {
                printf("[REC_ERR] Unknown error code: %d\n", info->err_code);
            }
            printf("[REC_ERR] File: %s, Line: %d\n", info->file, info->line);
        }
        idx = (idx + 1) % REC_ERR_QUEUE_MAX;
    }
}

// 对外唯一错误上报接口，返回错误码
uint8_t rec_err(uint8_t err_code) {
    rec_err_report(err_code, __FILE__, __LINE__);
    return err_code;
}
