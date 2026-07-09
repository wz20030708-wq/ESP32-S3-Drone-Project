<div align="center">

# 🚁 Flix 飞控 — ESP32-S3 四旋翼无人机固件

**基于 FreeRTOS 双核多任务架构的完整四旋翼飞行控制系统**

![ESP32-S3](https://img.shields.io/badge/MCU-ESP32--S3-blue?style=flat-square&logo=espressif)
![Arduino](https://img.shields.io/badge/Framework-Arduino-00979D?style=flat-square&logo=arduino)
![FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS-green?style=flat-square)
![MAVLink](https://img.shields.io/badge/Protocol-MAVLink-FF6600?style=flat-square)

[🧬 项目概述](#-项目概述) •
[📦 硬件引脚](#-硬件引脚定义) •
[🏗️ 系统架构](#️-系统架构) •
[🎮 飞行控制](#-飞行控制) •
[📡 通信方式](#-通信方式) •
[🔧 构建指南](#-构建指南) •
[⚙️ 使用说明](#️-使用说明)

---

</div>

---

## 🧬 项目概述

**Flix 飞控** 是一个基于 **ESP32-S3** 微控制器的四旋翼无人机固件项目，实现了专业级飞行控制所需的所有核心功能：

| 特性 | 说明 |
|------|------|
| **姿态估计** | 互补滤波 / Madgwick AHRS 双模式切换 |
| **控制架构** | 4 层级联 PID（位置 → 速度 → 姿态角 → 角速度） |
| **飞行模式** | 增稳模式 (STAB) + 悬停模式 (HOVER) |
| **遥控协议** | SBUS 接收机，自动通道校准 |
| **地面站** | MAVLink 协议 over UDP，支持 QGroundControl |
| **图传** | OV3660 摄像头实时 JPEG Web 预览 |
| **传感器** | MPU9250 (IMU) + BMP280 (气压计) + VL53L0X (激光测距) |
| **数据记录** | RAM 环形日志，100Hz 采样，10 秒循环记录 |
| **故障保护** | RC 信号丢失自动降落 |
| **参数管理** | NVS 闪存存储，飞行中在线调整 |



---

## 📦 硬件引脚定义

### SPI — IMU (MPU6500)

| 信号 | GPIO | 说明 |
|------|:----:|------|
| SCK  | 38   | SPI 时钟 |
| MISO | 36   | SPI 数据输入 |
| MOSI | 37   | SPI 数据输出 |
| CS   | 35   | 片选 |

### I2C — 传感器总线

| 信号 | GPIO | 说明 |
|------|:----:|------|
| SDA  | 47   | I2C 数据线 |
| SCL  | 21   | I2C 时钟线 |

> 总线频率 **400kHz** (Fast Mode)  
> 挂载设备: BMP280 气压计 (地址 `0x76`) + VL53L0X 激光测距 (地址 `0x29`)

### 电机输出 (PWM — LEDC)

| 电机 | 引脚 | 位置 |
|:----:|:----:|:----:|
| M0   | 41   | 后左 (Rear Left) |
| M1   | 40   | 后右 (Rear Right) |
| M2   | 39   | 前右 (Front Right) |
| M3   | 42   | 前左 (Front Left) |

**当前配置** (直流 MOSFET 驱动): **20kHz / 10位**  
**备选配置** (ESC 无刷电机驱动): **400Hz / 16位**

### 摄像头 (OV3660)

OV3660 使用标准的 ESP32S3-EYE 并行接口引脚配置。

| 信号  | GPIO | 信号    | GPIO |
|-------|:----:|---------|:----:|
| PCLK  | 13   | VSYNC   | 6    |
| HREF  | 7    | XCLK    | 15   |
| SDA   | 4    | SCL     | 5    |
| D0~D3 | 11,9,8,10 | D4~D7 | 12,18,17,16 |

### 其他外设

| 功能 | 引脚 | 说明 |
|------|:----:|------|
| SBUS RC 接收 | 14 (UART2 RX) | SBUS 协议 |
| 电机电压监测 | 1 (ADC1_CH0) | 12位, 11dB 衰减 |
| 芯片电压监测 | 2 (ADC1_CH1) | 12位, 11dB 衰减 |
| 板载 LED | 48 | 状态指示灯 |

---

## 🏗️ 系统架构

### FreeRTOS 三任务双核调度

```
┌─────────────────────────────────────────────────────────────┐
│                      ESP32-S3 双核                          │
├──────────────────── Core 1 ──────────────────── Core 0 ────┤
│                                                        │
│  ┌─────────────────────┐  ┌─────────────────────┐       │
│  │   FlightCtrl Task   │  │     CommTask        │       │
│  │  优先级 24 (最高)    │  │  优先级 5 (中等)     │       │
│  │  栈 8KB  │  ~1kHz   │  │  栈 8KB  │  ~200Hz  │       │
│  │                     │  │                     │       │
│  │  • 读取 IMU         │  │  • 串口 CLI 处理     │       │
│  │  • 读取 RC          │  │  • Web 服务器        │       │
│  │  • 姿态估计         │  │  • MAVLink 通信      │       │
│  │  • 控制计算         │  │                     │       │
│  │  • 输出电机         │  │                     │       │
│  └─────────────────────┘  └─────────────────────┘       │
│                                                        │
│                             ┌─────────────────────┐       │
│                             │     BgTask           │       │
│                             │  优先级 1 (最低)     │       │
│                             │  栈 4KB  │  ~100Hz   │       │
│                             │                     │       │
│                             │  • 电压监测 (5Hz)    │       │
│                             │  • 传感器读取 (2Hz)  │       │
│                             │  • 摄像头 (7Hz)      │       │
│                             │  • 日志记录          │       │
│                             │  • 参数同步          │       │
│                             │  • LED 状态指示      │       │
│                             └─────────────────────┘       │
└─────────────────────────────────────────────────────────────┘
```

| 任务 | 核心 | 优先级 | 栈大小 | 频率 | 职责 |
|:----|:----:|:------:|:------:|:----:|------|
| **FlightCtrl** | Core 1 | 24 (最高) | 8 KB | ~1 kHz | IMU → RC → 姿态估计 → 控制 → 电机输出 |
| **CommTask** | Core 0 | 5 (中等) | 8 KB | ~200 Hz | CLI + Web 服务器 + MAVLink |
| **BgTask** | Core 0 | 1 (最低) | 4 KB | ~100 Hz | 传感器、摄像头、日志、LED |

### 模块文件结构

```
RC_QGC_photo_wifi/
├── 📄 RC_QGC_photo_wifi.ino    # 主入口: setup() / loop() + 任务创建 + CPU 监控
├── 📄 globals.h                 # 全局 extern 变量声明
│
├── ⚙️ 飞行控制
│   ├── control.ino              # 姿态控制 + 电机混控 + 悬停控制器
│   ├── estimate.ino             # 姿态估计 (互补滤波 / Madgwick)
│   ├── motors.ino               # 电机 PWM 驱动 (4路 LEDC)
│   ├── imu.ino                  # IMU 驱动 (MPU9250, SPI)
│   ├── rc.ino                   # SBUS 遥控接收 + 自动通道校准
│   ├── sensors.ino              # I2C 传感器 (BMP280 + VL53L0X)
│   ├── safety.ino               # 故障安全 (RC 丢失自动降落)
│   └── time.ino                 # 全局时间管理 + 循环频率统计
│
├── 📡 通信
│   ├── wifi.ino                 # WiFi STA + UDP + Web 服务器 + 监控页面
│   ├── mavlink.ino              # MAVLink 协议 (UDP, QGC 地面站)
│   └── cli.ino                  # 串口命令行 + 参数管理
│
├── 🖥️ 外设
│   ├── camera.ino               # OV3660 摄像头驱动
│   ├── led.ino                  # 状态指示灯
│   └── voltage.ino              # ADC 电压测量
│
├── 💾 数据
│   ├── parameters.ino           # 参数存储 (NVS 闪存)
│   └── log.ino                  # RAM 环形日志 (100Hz, 10s)
│
└── 📐 数学库
    ├── vector.h                 # 三维向量类
    ├── quaternion.h             # 旋转四元数类
    ├── pid.h                    # PID 控制器类
    ├── lpf.h                    # 低通 / 陷波滤波器
    ├── madgwick.h               # Madgwick AHRS 算法
    ├── hover_controller.h       # 级联悬停控制器
    └── util.h                   # 工具函数
```

---

## 🎮 飞行控制

### 4 层级联 PID 控制

```
                           位置环         速度环         姿态角环       角速度环
                          ┌────────┐   ┌────────┐   ┌────────┐   ┌────────┐
   摇杆/自动 ──────────▶ │  P    │──▶│ PID   │──▶│ PID   │──▶│ PID   │──▶ 混控器 ──▶ 电机
                          │ 环    │   │ 环    │   │ 环    │   │ 环    │
                          └────────┘   └────────┘   └────────┘   └────────┘
                             ↑            ↑            ↑            ↑
                          位置反馈     速度反馈     姿态角反馈    角速度反馈
```

| 控制层级 | 算法 | 数量 | 说明 |
|:---------|:----|:----:|------|
| 位置环 | P | 2 | 水平位置控制 |
| 速度环 | PID | 2 | 水平速度控制 (含加速度前馈) |
| 姿态角环 | PID | 3 | 横滚 / 俯仰 / 偏航 |
| 角速度环 | PID | 3 | 横滚 / 俯仰 / 偏航 |

### 飞行模式

#### STAB (增稳模式)
- 方向摇杆 → 目标姿态角 (横滚/俯仰 ±30°)
- 偏航摇杆 → 偏航角速率
- 松杆自动回平

#### HOVER
- 方向摇杆 → 水平速度指令 (最大 1.5 m/s)
- 油门杆居中 → 定高
- 油门杆偏离 → 垂直速度控制 (最大 1.0 m/s)
- 偏航摇杆 → 偏航角速率
- 松杆 → 自动悬停

### 姿态估计

**互补滤波 (默认)**
- 陀螺仪积分预测姿态 + 加速度计重力向量修正
- 飞行中自动限制修正幅度（防止机动干扰）

**Madgwick AHRS** (通过 `#define USE_MADGWICK_FILTER` 启用)
- 基于梯度下降的六轴融合算法

### 安全保护
- **低油门积分保护**: 油门 < 0.12 时清除所有积分项
- **RC 超时保护**: 1 秒无信号自动降落 (10 秒平滑减速)
- **BrownOut 禁用**: `disableBrownOut()` 防止电压跌落复位
- **模式切换防抖**: 3 次连续采样确认模式切换
- **数据有效性检查**: NAN / INF 贯穿控制链路

---

## 📡 通信方式

### 1. Web 监控页面

连接 WiFi 后在浏览器访问 `http://192.168.1.100`，可查看:

```
┌─────────────────────────────────────────────────┐
│   🚁 飞行监控                                    │
├────────────────────┬────────────────────────────┤
│                    │  横滚:  -0.5°               │
│   [ 摄像头画面 ]    │  俯仰:   1.2°               │
│   OV3660 实时预览   │  偏航:  45.3°               │
│                    │  高度:   2.34 m              │
│                    │  电池:  11.48 V              │
│                    │  CPU:   34%                  │
│                    ├────────────────────────────┤
│                    │  M0: ████████░░  ██%          │
│                    │  M1: ██████░░░░  ██%          │
│                    │  M2: ███████░░░  ██%          │
│                    │  M3: ████████░░  ██%          │
└────────────────────┴────────────────────────────┘
```

- 摄像头画面 150ms 轮询更新
- 实时传感器仪表盘
- 电机输出可视化进度条
- CPU 使用率监控

### 2. MAVLink 地面站

通过 UDP 端口 `14550` 与 **QGroundControl** 通信:

- 实时姿态、位置、速度上报
- 参数读写 (PID 在线调参)
- 解锁 / 上锁控制
- 飞行模式切换

### 3. 串口命令行

通过串口 `115200 baud` 提供 20+ 条调试命令:

| 命令 | 说明 |
|:----|------|
| `help` | 显示帮助信息 |
| `info` | 系统状态概览 |
| `calib_acc` | 加速度计六面校准 |
| `calib_gyro` | 陀螺仪零偏校准 |
| `param list` | 列出所有参数 |
| `param set <name> <value>` | 修改参数 |
| `log dump` | 导出飞行日志 |
| `motor test <n>` | 测试单个电机 |
| `reboot` | 重启飞控 |

---

## 🔧 构建指南

### 硬件要求

| 组件 | 型号 | 数量 |
|------|------|:----:|
| 飞控板 | ESP32-S3 Dev Module / WeMOS D1 MINI ESP32 | 1 |
| IMU | MPU9250 (SPI) | 1 |
| 气压计 | BMP280 (I2C) | 1 |
| 激光测距 | VL53L0X (I2C) | 1 |
| 摄像头 | OV3660 | 1 |
| 电机 ×4 | 直流有刷 / 无刷 (ESC) | 4 |
| RC 接收机 | SBUS 协议 | 1 |

### 软件依赖

| 库 | 用途 | 安装方式 |
|:---|:----|:---------|
| [FlixPeriph](https://github.com/okalachev/FlixPeriph) | MPU9250 SPI 驱动 | Arduino 库管理器 |
| [SBUS](https://github.com/okalachev/SBUS) | SBUS 协议解析 | Arduino 库管理器 |
| [MAVLink](https://github.com/mavlink/c_library_v2) | MAVLink v2 协议 | Arduino 库管理器 |
| [VL53L0X](https://github.com/pololu/vl53l0x-arduino) | 激光测距驱动 | Arduino 库管理器 |
| `esp_camera` | OV3660 驱动 | ESP32 Arduino 内置 |
| `WiFi` / `WebServer` / `WiFiUdp` | WiFi 通信 | ESP32 Arduino 内置 |
| `SPI` / `Wire` | 总线通信 | ESP32 Arduino 内置 |
| `Preferences` | NVS 存储 | ESP32 Arduino 内置 |

### 编译步骤

#### Arduino IDE

1. 安装 [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32) (≥ 2.0.x)
2. 通过库管理器安装: `FlixPeriph`、`SBUS`、`MAVLink`、`VL53L0X`
3. 选择开发板: `ESP32S3 Dev Module`
4. 选择 **PSRAM: "OPI PSRAM"**
5. 烧录上传

#### PlatformIO

```ini
[env:esp32-s3-dev]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps =
    FlixPeriph
    SBUS
    MAVLink
    VL53L0X
board_build.arduino.memory_type = qio_opi
```

### 编译宏

在 `RC_QGC_photo_wifi.ino` 头部配置:

| 宏 | 默认值 | 说明 |
|:---|:------:|------|
| `WIFI_ENABLED` | `1` | 启用 WiFi + MAVLink |
| `CAMERA_ENABLED` | `1` | 启用摄像头 |
| `USE_MADGWICK_FILTER` | (未定义) | 启用 Madgwick AHRS |

---

## ⚙️ 使用说明

### 快速上手

1. **硬件连接**: 按引脚定义连接所有外设
2. **烧录固件**: 编译并上传到 ESP32-S3
3. **上电**: LED 指示灯快闪表示初始化中
4. **校准**: 通过串口 CLI 进行加速度计校准
   ```
   calib_acc    # 六面校准 (约 48 秒)
   ```
5. **连接地面站**: 打开 QGC，会自动检测到 `192.168.1.100:14550`
6. **试飞**: 解锁 → 缓慢推油门 → 起飞

### 参数调整

通过 CLI 或 MAVLink 在线调整 PID 参数:

```
param set roll_rate_p 0.15
param set roll_rate_i 0.01
param set roll_rate_d 0.002
param list             # 查看所有参数
```

飞行结束后，使用 `log dump` 导出飞控日志进行分析。

### WiFi 配置

编辑 [wifi.ino](wifi.ino) 中的网络配置:

```cpp
#define WIFI_SSID      "你的WiFi名称"
#define WIFI_PASSWORD  "你的WiFi密码"
#define WIFI_STATIC_IP "192.168.1.100"
```

---

