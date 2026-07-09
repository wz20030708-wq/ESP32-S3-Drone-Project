/**
 * @file globals.h
 * @brief 全局变量集中声明
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2025
 * @version 1.0
 * 
 * 集中管理所有跨模块的 extern 全局变量声明。
 * 各 .ino 模块通过包含此头文件即可访问所有全局变量，
 * 无需在每个文件中重复声明 extern。
 */

#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <WebServer.h>
#include "vector.h"
#include "quaternion.h"
#include "lpf.h"
#include "pid.h"

// ============================================================
// 来自 RC_QGC_photo_wifi.ino — 主固件文件
// ============================================================

/** 飞行控制任务句柄 */
extern TaskHandle_t flightControlTaskHandle;
/** 通信任务句柄 */
extern TaskHandle_t commTaskHandle;
/** 后台任务句柄 */
extern TaskHandle_t backgroundTaskHandle;

/** 互斥锁: 保护跨核心共享的关键飞行状态数据 */
extern SemaphoreHandle_t mutexState;
/** 互斥锁: 保护串口输出 */
extern SemaphoreHandle_t mutexSerial;

/** 全局时间戳 (s) */
extern float t;
/** 时间步长 (s) */
extern float dt;

/** 横滚控制量 (-1~1) */
extern float controlRoll;
/** 俯仰控制量 (-1~1) */
extern float controlPitch;
/** 偏航控制量 (-1~1) */
extern float controlYaw;
/** 油门控制量 (0~1) */
extern float controlThrottle;
/** 飞行模式控制量 */
extern float controlMode;

/** 陀螺仪原始数据 (rad/s) */
extern Vector gyro;
/** 加速度计原始数据 (m/s²) */
extern Vector acc;
/** 滤波后的角速度 (rad/s) */
extern Vector rates;

/** 陀螺仪运行时零偏 (rad/s) */
extern Vector gyroBias;

/** 当前姿态四元数 */
extern Quaternion attitude;
/** 是否处于着陆状态 */
extern bool landed;

/** 垂直速度估计 (m/s, 正值=上升) */
extern float verticalVelocity;
/** 滤波后气压高度 (m) */
extern float altFiltered;
/** 定高目标高度 (m) */
extern float altitudeTarget;
/** 悬停油门基准值 (0~1) */
extern float hoverThrottle;

/** 四个电机输出 (0~1) */
extern float motors[4];

// ============================================================
// 来自 rc.ino — RC遥控接收模块
// ============================================================

/** 原始RC通道数据 */
extern uint16_t channels[16];
/** 上次控制更新时间 (s) */
extern float controlTime;
/** 通道校准零点 */
extern float channelZero[16];
/** 通道校准最大值 */
extern float channelMax[16];

/** 横滚通道映射 */
extern float rollChannel;
/** 俯仰通道映射 */
extern float pitchChannel;
/** 油门通道映射 */
extern float throttleChannel;
/** 偏航通道映射 */
extern float yawChannel;
/** 模式通道映射 */
extern float modeChannel;

// ============================================================
// 来自 time.ino — 时间管理模块
// ============================================================

/** 主循环频率 (Hz) */
extern float loopRate;

// ============================================================
// 来自 sensors.ino — I2C传感器模块
// ============================================================

/** PM280气压传感器是否正常 */
extern bool pm280Ok;
/** VL53L0X激光测距传感器是否正常 */
extern bool vl53l0xOk;
/** 气压值 (hPa) */
extern float pressureHpa;
/** 激光测距距离 (mm) */
extern float distanceMm;
/** 海拔高度 (m) */
extern float altitudeM;
/** 温度 (°C) */
extern float temperatureC;

// ============================================================
// 来自 voltage.ino — ADC电压测量模块
// ============================================================

/** 电机电源电压 (V) */
extern float motorVoltage;
/** 芯片供电电压 (V) */
extern float chipVoltage;

// ============================================================
// 来自 motors.ino — 电机驱动控制模块
// ============================================================

/** 电机索引: 后左 */
extern const int MOTOR_REAR_LEFT;
/** 电机索引: 后右 */
extern const int MOTOR_REAR_RIGHT;
/** 电机索引: 前右 */
extern const int MOTOR_FRONT_RIGHT;
/** 电机索引: 前左 */
extern const int MOTOR_FRONT_LEFT;

// ============================================================
// 来自 control.ino — 飞行控制模块
// ============================================================

/** 飞行模式: 姿态稳定 */
extern const int STAB;
/** 飞行模式: 避障 */
extern const int OBSTACLE;

/** 当前飞行模式 */
extern int mode;
/** 解锁状态 */
extern bool armed;
/** 高度位置环PID (外环) */
extern PID altPosPID;
/** 垂直速度环PID (内环) */
extern PID altVelPID;

/** 悬停性能监控开关 (0.0=关闭, 1.0=开启) */
extern float hoverPerfLog;
/** Madgwick滤波器增益 */
extern float madgwickBeta;

/** 陀螺仪陷波滤波器 */
extern NotchFilter<Vector> gyroNotchFilter;

// ============================================================
// 来自 mavlink.ino — MAVLink协议通信模块 (WiFi)
// ============================================================

#if WIFI_ENABLED

/** MAVLink连接状态 */
extern bool mavlinkConnected;
/** 上次MAVLink消息接收时间 (s) */
extern float lastMavlinkTime;

#endif // WIFI_ENABLED

// ============================================================
// 来自 wifi.ino — Wi-Fi通信与Web监控模块 (WiFi)
// ============================================================

#if WIFI_ENABLED

/** Web服务器对象 */
extern WebServer server;

#endif // WIFI_ENABLED

// ============================================================
// 来自 camera.ino — 摄像头驱动模块 (Camera)
// ============================================================

#if CAMERA_ENABLED

/** 摄像头就绪标志 */
extern bool cameraReady;
/** 帧序列号 */
extern uint32_t cameraJpgSeq;
/** 摄像头捕获处理函数 */
extern void handleCameraCapture();
/** 摄像头状态查询函数 */
extern void handleCameraStatus();

#endif // CAMERA_ENABLED