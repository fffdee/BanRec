# BanRec - 录制/播放模块重构

## 架构概述

本次重构实现了完全解耦的音频录制/播放架构，具有以下特点：

### 核心特性

1. **接口完全解耦** - 通过回调函数实现，无硬编码依赖
2. **多数据源混音** - 支持多个输入源的实时混音（叠加/平均/单源模式）
3. **回调式数据输出** - 播放通过回调获取数据，方便与其他模块混音
4. **HAL层抽象** - 底层接口可提前配置，便于跨平台移植
5. **代码整洁** - 清晰的层次结构，职责分明
6. **EMMC存储支持** - 默认使用EMMC作为存储接口

## 目录结构

```
BanRec/
├── rec_config.h          # 全局配置和类型定义
├── core/                 # 核心层
│   ├── rec_core.h/c      # 录制/播放核心逻辑
│   └── rec_err_handle.h/c # 错误处理
├── hal/                  # 硬件抽象层
│   ├── rec_hal.h/c       # HAL管理接口
│   ├── play/             # 播放硬件（ALSA）
│   └── input_handle/     # 输入硬件（键盘）
├── interface/            # 接口适配层
│   └── rec_interface.h/c # HAL适配器（回调实现）
├── api/                  # API层
│   ├── rec_api.h/c       # 对外API接口
│   └── rec_test.h/c      # 测试接口
└── main.c                # 主程序
```

## 层次关系

```
┌─────────────────────────────────────────┐
│          Application (main.c)           │
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│         API Layer (rec_api)             │  ← 简洁的对外接口
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│       Core Layer (rec_core)             │  ← 核心逻辑（混音、状态管理）
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│    Interface Layer (rec_interface)      │  ← HAL适配（回调实现）
└──────────────────┬──────────────────────┘
                   │
┌──────────────────▼──────────────────────┐
│      HAL Layer (rec_hal, drivers)       │  ← 硬件抽象
└─────────────────────────────────────────┘
```

## 快速开始

### 1. 初始化

```c
#include "rec_api.h"

// 初始化模块
rec_api_init();
```

### 2. 录制音频（单数据源）

```c
// 创建输入源（IIS麦克风，音量100%）
rec_source_config_t iis_source = rec_interface_create_iis_input(100);
rec_api_recorder_add_source(&iis_source);

// 设置输出文件
rec_api_recorder_set_output("recording.wav");

// 启动录制
rec_api_recorder_start();

// 周期性处理（在主循环或定时器中调用）
rec_api_recorder_process();

// 停止录制
rec_api_recorder_stop();
```

### 3. 多数据源混音录制

```c
// 添加多个输入源
rec_source_config_t source1 = rec_interface_create_iis_input(80);  // 音量80%
rec_source_config_t source2 = rec_interface_create_iis_input(60);  // 音量60%

rec_api_recorder_add_source(&source1);
rec_api_recorder_add_source(&source2);

// 设置混音模式
rec_api_recorder_set_mix_mode(REC_MIX_MODE_AVG);  // 平均混音

// 其余步骤同上...
```

### 4. 播放音频（通过回调输出）

```c
// 播放输出回调
uint8_t play_output_callback(const int16_t *buf, uint16_t len, void *user_data) {
    // 将数据发送到播放设备
    PlayCallback((uint16_t*)buf);
    return 0;
}

// 设置播放输入源
rec_api_player_set_input("recording.wav");

// 设置输出回调
rec_api_player_set_output_callback(play_output_callback, NULL);

// 启动播放
rec_api_player_start();

// 周期性处理
rec_api_player_process();

// 停止播放
rec_api_player_stop();
```

### 5. 自定义数据源（回调方式）

```c
// 自定义读取回调
uint16_t my_read_callback(int16_t *buf, uint16_t samples, void *user_data) {
    // 填充数据到buf
    // ...
    return samples;  // 返回实际读取的样本数
}

// 创建自定义数据源
rec_source_config_t custom_source = rec_interface_create_custom_source(
    my_read_callback,  // 读取回调
    NULL,              // 写入回调
    NULL,              // 启动回调
    NULL,              // 停止回调
    my_user_data,      // 用户数据
    100                // 音量
);

// 添加到录制器
rec_api_recorder_add_source(&custom_source);
```

## 混音模式

支持3种混音模式：

- **REC_MIX_MODE_NONE** - 只使用第一个有效数据源
- **REC_MIX_MODE_ADD** - 叠加混音（所有源相加，注意溢出）
- **REC_MIX_MODE_AVG** - 平均混音（所有源平均，推荐）

```c
rec_api_recorder_set_mix_mode(REC_MIX_MODE_AVG);
```

## 数据源类型

### 1. IIS输入源
```c
rec_source_config_t iis_in = rec_interface_create_iis_input(100);
```

### 2. IIS输出源
```c
rec_source_config_t iis_out = rec_interface_create_iis_output(100);
```

### 3. EMMC录音输出
```c
rec_source_config_t emmc_out = rec_interface_create_emmc_record_output("file.wav");
```

### 4. EMMC播放输入
```c
rec_source_config_t emmc_in = rec_interface_create_emmc_play_input("file.wav");
```

### 5. 自定义回调源
```c
rec_source_config_t custom = rec_interface_create_custom_source(
    read_cb, write_cb, start_cb, stop_cb, user_data, volume
);
```

## 状态管理

流对象状态机：

```
UNINIT → READY → START → RUNNING → PAUSED → RUNNING
                              ↓
                          STOPPED
```

控制函数：
```c
rec_api_recorder_start();   // 启动
rec_api_recorder_pause();   // 暂停
rec_api_recorder_resume();  // 恢复
rec_api_recorder_stop();    // 停止
```

## 配置选项

在 `rec_config.h` 中配置：

```c
#define REC_SAMPLE_RATE         48000   // 采样率
#define REC_SAMPLE_WIDTH        16      // 采样宽度（位）
#define REC_CHANNELS            2       // 声道数
#define REC_INPUT_SOURCE_MAX    4       // 最大输入源数量
#define REC_FRAME_SIZE_MS       1       // 帧大小（毫秒）
```

## 移植指南

### 移植到新平台

1. **实现HAL层操作函数** - 在 `rec_interface.c` 中实现对应平台的回调
2. **配置硬件参数** - 修改 `rec_config.h` 中的参数
3. **适配存储接口** - 实现EMMC/Flash的读写函数

示例：添加新的NOR Flash支持

```c
// 在rec_interface.c中添加
static uint16_t nor_read_cb(int16_t *buf, uint16_t samples, void *user_data) {
    // 从NOR Flash读取数据
    // ...
    return samples;
}

static uint8_t nor_write_cb(const int16_t *buf, uint16_t len, void *user_data) {
    // 写入NOR Flash
    // ...
    return 0;
}

// 在rec_interface.h中声明
rec_source_config_t rec_interface_create_nor_source(uint32_t addr);
```

## 编译

```bash
mkdir build && cd build
cmake ..
make
```

## 运行示例

```bash
# 简单录制
./BanRec 1

# 多源混音录制
./BanRec 2

# 播放文件
./BanRec 3

# 监听模式
./BanRec 4

# 自定义源
./BanRec 5
```

## 优势总结

✅ **完全解耦** - 各层通过回调接口通信，无直接依赖  
✅ **易于扩展** - 新增数据源只需添加回调实现  
✅ **灵活混音** - 支持多种混音模式和动态音量调节  
✅ **便于移植** - HAL层抽象，只需适配底层驱动  
✅ **代码整洁** - 清晰的层次结构，职责分明  
✅ **回调输出** - 播放数据通过回调获取，便于二次混音  

## 注意事项

1. 所有process函数需要周期性调用（建议1ms周期）
2. 多线程环境需要添加互斥保护
3. 音量值范围：0-100
4. WAV文件头在停止时自动写入
5. 缓冲区大小根据REC_FRAME_SAMPLES计算

## 后续扩展

- [ ] 添加音频效果处理（EQ、压缩、混响等）
- [ ] 支持实时音量监控
- [ ] 添加多文件播放列表
- [ ] 实现循环录制（环形缓冲）
- [ ] 支持更多音频格式（MP3、AAC等）

---

**作者**: BanGO  
**日期**: 2026-02-05  
**版本**: 2.0 (重构版)
