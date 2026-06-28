/**
 * @file RC_QGC_photo_wifi.ino
 * @brief ESP32无人机主固件文件
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2025-05-30
 * @version 1.0
 * 
 * 基于FreeRTOS多任务架构的四旋翼无人机飞控固件。
 * 开发板: ESP32 Dev Module 或 WeMOS D1 MINI ESP32
 * 
 * 架构说明:
 * - 双核FreeRTOS任务调度，飞行控制为最高优先级
 * - Core 1: 飞行控制任务(最高优先级)
 * - Core 0: 通信任务(中等优先级) + 后台任务(最低优先级)
 */

#include "vector.h"
#include "quaternion.h"
#include "util.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

/** 是否启用WiFi功能 */
#define WIFI_ENABLED 1
/** 是否启用摄像头功能 */
#define CAMERA_ENABLED 1

// ==================== FreeRTOS 对象 ====================

/** 飞行控制任务句柄 */
TaskHandle_t flightControlTaskHandle = NULL;
/** 通信任务句柄 */
TaskHandle_t commTaskHandle = NULL;
/** 后台任务句柄 */
TaskHandle_t backgroundTaskHandle = NULL;

/** 互斥锁: 保护跨核心/跨任务共享的关键飞行状态数据 */
SemaphoreHandle_t mutexState = NULL;

/** 互斥锁: 保护Serial.print()和MAVLink print buffer避免输出交织 */
SemaphoreHandle_t mutexSerial = NULL;

// ==================== 全局变量 ====================

/** 全局时间戳(s) */
float t = NAN;
/** 时间步长(s) */
float dt;

/** 横滚控制量(-1~1) */
float controlRoll;
/** 俯仰控制量(-1~1) */
float controlPitch;
/** 偏航控制量(-1~1) */
float controlYaw;
/** 油门控制量(0~1) */
float controlThrottle;

/** 飞行模式控制量 */
float controlMode = NAN;

/** 陀螺仪原始数据(rad/s) */
Vector gyro;
/** 加速度计原始数据(m/s²) */
Vector acc;
/** 滤波后的角速度(rad/s) */
Vector rates;

/** 当前姿态四元数 */
Quaternion attitude;
/** 是否处于着陆状态 */
bool landed;

/** 四个电机输出(0~1) */
float motors[4];

// ==================== FreeRTOS 任务函数声明 ====================

/** 飞行控制任务(Core 1, 最高优先级) */
void flightControlTask(void *pvParameters);
/** 通信任务(Core 0, 中等优先级) */
void commTask(void *pvParameters);
/** 后台任务(Core 0, 最低优先级) */
void backgroundTask(void *pvParameters);

// ==================== 初始化函数 ====================

/**
 * @brief Arduino初始化函数
 * 
 * 初始化系统硬件、传感器、通信模块，并创建FreeRTOS任务。
 */
void setup() {
	Serial.begin(115200);
	delay(50);

	mutexState = xSemaphoreCreateMutex();
	configASSERT(mutexState != NULL);

	mutexSerial = xSemaphoreCreateMutex();
	configASSERT(mutexSerial != NULL);

	disableBrownOut();
	setupParameters();
	setupLED();
	setupSensors();
	print("I2C传感器就绪\n");
#if CAMERA_ENABLED
	setupCamera();
#endif
	setupMotors();

	setLED(true);
#if WIFI_ENABLED
	setupWiFi();
	print("WiFi + Web Server ready\n");
#endif
	setupIMU();
	print("IMU初始化完成\n");
	setupADC();
	print("ADC电压测量就绪\n");
	setupRC();
	setLED(false);

	print("创建自由RTOS任务...\n");

	// ==================== 创建 FreeRTOS 任务 ====================

	// 飞控任务 - Core 1, 最高优先级
	xTaskCreatePinnedToCore(
		flightControlTask,
		"FlightCtrl",
		8192,
		NULL,
		24,
		&flightControlTaskHandle,
		1
	);

	// 通信任务 - Core 0, 中等优先级
	xTaskCreatePinnedToCore(
		commTask,
		"CommTask",
		8192,
		NULL,
		5,
		&commTaskHandle,
		0
	);

	// 后台任务 - Core 0, 最低优先级
	xTaskCreatePinnedToCore(
		backgroundTask,
		"BgTask",
		4096,
		NULL,
		1,
		&backgroundTaskHandle,
		0
	);

	print("FreeRTOS 多任务架构初始化完成！！！\n");
	print("Flix 飞控运行中\n");
}

// ==================== Arduino loop ====================

/**
 * @brief Arduino主循环
 * 
 * FreeRTOS模式下仅做延时，实际工作由各任务完成。
 */
void loop() {
	vTaskDelay(pdMS_TO_TICKS(100));
}

// ==================== 飞控任务 (Core 1, 最高优先级) ====================

/**
 * @brief 飞行控制主任务
 * 
 * 执行频率最高的任务，包含完整的飞行控制闭环：
 * 读取IMU → 更新时间 → 读取遥控 → 姿态估计 → 控制计算 → 发送电机
 * 
 * @param pvParameters 任务参数(未使用)
 */
void flightControlTask(void *pvParameters) {
	while (1) {
		readIMU();
		step();
		readRC();
		estimate();
		control();
		sendMotors();

		taskYIELD();
	}
}

// ==================== 通信任务 (Core 0, 中等优先级) ====================

/**
 * @brief 通信任务
 * 
 * 处理串口输入、Web服务器和MAVLink通信。
 * 
 * @param pvParameters 任务参数(未使用)
 */
void commTask(void *pvParameters) {
	while (1) {
		handleInput();
#if WIFI_ENABLED
		handleWebServer();
		processMavlink();
#endif
		vTaskDelay(pdMS_TO_TICKS(5));
	}
}

// ==================== 后台任务 (Core 0, 最低优先级) ====================

/**
 * @brief 后台任务
 * 
 * 执行低优先级任务：电压监测、传感器读取、摄像头采集、数据日志等。
 * 
 * @param pvParameters 任务参数(未使用)
 */
void backgroundTask(void *pvParameters) {
	TickType_t lastWakeTime = xTaskGetTickCount();
	int voltageTick = 0;
	int sensorTick = 0;
	int cameraTick = 0;

	while (1) {
		voltageTick++;
		if (voltageTick >= 500) {
			readVoltages();
			voltageTick = 0;
		}
		sensorTick++;
		if (sensorTick >= 50) {
			readSensors();
			sensorTick = 0;
		}
#if CAMERA_ENABLED
		cameraTick++;
		if (cameraTick >= 20) {
			cameraCapture();
			cameraTick = 0;
		}
#endif
		logData();
		syncParameters();
		blinkLED();

		vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(10));
	}
}