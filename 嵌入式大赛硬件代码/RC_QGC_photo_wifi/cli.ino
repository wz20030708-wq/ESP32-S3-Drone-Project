/**
 * @file cli.ino
 * @brief 命令行界面模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 实现通过串口与用户交互的命令行界面，支持参数管理、系统信息查询、校准等功能。
 * 所有操作均通过互斥锁保护，确保多任务环境下的线程安全。
 */

#include "pid.h"
#include "vector.h"
#include "util.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "globals.h"

float getParameter(const char *name);
bool setParameter(const char *name, float value);

/** 欢迎信息和命令帮助 */
const char* motd =
"\nWelcome to\n"
"                                      \n"
"    /\\      /\\      /\\      /\\    \n"
"   /  \\    /  \\    /  \\    /  \\   \n"
"  /__M_\\  /__i_\\  /__n_\\  /__i_\\  \n"
" /   F  \\/   L  \\/   i  \\/   x  \\ \n"
"/________\\_______\\_______\\_______\\ \n"
"                                       \n"
"输入命令，然后回车:\n"
"help - 显示帮助信息\n"
"p - 显示所有参数\n"
"p <name> - 显示指定参数\n"
"p <name> <value> - 设置参数\n"
"preset - 重置所有参数\n"
"mfr/mfl/mrr/mrl - 测试单个电机 (安全起见请勿安装桨叶！)\n"
"cr - 校准RC遥控器\n"
"ca - 校准加速度计\n"
"ps - 显示姿态(欧拉角)\n"
"rc - 显示RC遥控数据\n"
"psq - 显示姿态四元数\n"
"imu - 显示IMU数据\n"
"time - 显示时间信息\n"
"wifi - 显示Wi-Fi信息\n"
"mot - 显示电机输出\n"
"sys - 显示系统信息\n"
"stab - 切换到自稳模式\n"
"obstacle - 切换到障碍规避模式\n"
"arm - 解锁无人机\n"
"disarm - 锁定无人机\n"
"log [dump] - 打印日志\n"
"reset - 重置无人机状态\n"
"reboot - 重启无人机\n";

/**
 * @brief 线程安全的打印函数
 * 
 * 使用互斥锁保护串口输出和MAVLink打印缓冲区，避免多任务竞争。
 * 
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void print(const char* format, ...) {
	char buf[1000];
	va_list args;
	va_start(args, format);
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	xSemaphoreTake(mutexSerial, portMAX_DELAY);
	Serial.print(buf);
#if WIFI_ENABLED
	mavlinkPrint(buf);
#endif
	xSemaphoreGive(mutexSerial);
}

/**
 * @brief 延时函数(带任务调度)
 * 
 * 在延时期间继续执行主循环和消息处理，确保系统不会卡死。
 * 
 * @param duration 延时时间(秒)
 */
void pause(float duration) {
	float start = t;
	while (t - start < duration) {
		step();
		handleInput();
#if WIFI_ENABLED
		processMavlink();
#endif
		delay(50);
	}
}

/**
 * @brief 设置飞行模式(线程安全)
 * @param newMode 目标飞行模式
 */
void setFlightMode(int newMode) {
    xSemaphoreTake(mutexState, portMAX_DELAY);
    mode = newMode;
    xSemaphoreGive(mutexState);
}

/**
 * @brief 设置解锁状态(线程安全)
 * @param arm true为解锁，false为锁定
 */
void setArmed(bool arm) {
    xSemaphoreTake(mutexState, portMAX_DELAY);
    armed = arm;
    xSemaphoreGive(mutexState);
}

/**
 * @brief 执行命令
 * 
 * 解析并执行用户输入的命令，支持参数查询、系统控制、校准等操作。
 * 
 * @param str 命令字符串
 * @param echo 是否回显命令
 */
void doCommand(String str, bool echo = false) {
	String command, arg0, arg1;
	splitString(str, command, arg0, arg1);
	if (command.isEmpty()) return;

	if (echo) {
		print("> %s\n", str.c_str());
	}

	command.toLowerCase();

	if (command == "help" || command == "motd") {
		print("%s\n", motd);
	} else if (command == "p" && arg0 == "") {
		printParameters();
	} else if (command == "p" && arg0 != "" && arg1 == "") {
		print("%s = %g\n", arg0.c_str(), getParameter(arg0.c_str()));
	} else if (command == "p") {
		bool success = setParameter(arg0.c_str(), arg1.toFloat());
		if (success) {
			print("%s = %g\n", arg0.c_str(), arg1.toFloat());
		} else {
			print("Parameter not found: %s\n", arg0.c_str());
		}
	} else if (command == "preset") {
		resetParameters();
	} else if (command == "time") {
		print("Time: %f\n", t);
		print("Loop rate: %.0f\n", loopRate);
		print("dt: %f\n", dt);
	} else if (command == "ps") {
		xSemaphoreTake(mutexState, pdMS_TO_TICKS(5));
		Vector a = attitude.toEuler();
		xSemaphoreGive(mutexState);
		print("roll: %f pitch: %f yaw: %f\n", degrees(a.x), degrees(a.y), degrees(a.z));
	} else if (command == "psq") {
		xSemaphoreTake(mutexState, pdMS_TO_TICKS(5));
		float qw = attitude.w, qx = attitude.x, qy = attitude.y, qz = attitude.z;
		xSemaphoreGive(mutexState);
		print("qw: %f qx: %f qy: %f qz: %f\n", qw, qx, qy, qz);
	} else if (command == "imu") {
		printIMUInfo();
		printIMUCalibration();
		xSemaphoreTake(mutexState, pdMS_TO_TICKS(5));
		bool _landed = landed;
		xSemaphoreGive(mutexState);
		print("landed: %d\n", _landed);
	} else if (command == "arm") {
			setArmed(true);
		} else if (command == "disarm") {
			setArmed(false);
		} else if (command == "stab") {
			setFlightMode(STAB);
		} else if (command == "obstacle") {
			setFlightMode(OBSTACLE);
		} else if (command == "rc") {
		print("channels: ");
		for (int i = 0; i < 16; i++) {
			print("%u ", channels[i]);
		}
		print("\nroll: %g pitch: %g yaw: %g throttle: %g mode: %g\n",
			controlRoll, controlPitch, controlYaw, controlThrottle, controlMode);
		print("mode: %s\n", getModeName());
		print("armed: %d\n", armed);
	} else if (command == "wifi") {
#if WIFI_ENABLED
		printWiFiInfo();
#endif
	} else if (command == "mot") {
		xSemaphoreTake(mutexState, pdMS_TO_TICKS(5));
		float mfr = motors[MOTOR_FRONT_RIGHT];
		float mfl = motors[MOTOR_FRONT_LEFT];
		float mrr = motors[MOTOR_REAR_RIGHT];
		float mrl = motors[MOTOR_REAR_LEFT];
		xSemaphoreGive(mutexState);
		print("front-right %g front-left %g rear-right %g rear-left %g\n", mfr, mfl, mrr, mrl);
	} else if (command == "log") {
		printLogHeader();
		if (arg0 == "dump") printLogData();
	} else if (command == "cr") {
		calibrateRC();
	} else if (command == "ca") {
		calibrateAccel();
	} else if (command == "mfr") {
		testMotor(MOTOR_FRONT_RIGHT);
	} else if (command == "mfl") {
		testMotor(MOTOR_FRONT_LEFT);
	} else if (command == "mrr") {
		testMotor(MOTOR_REAR_RIGHT);
	} else if (command == "mrl") {
		testMotor(MOTOR_REAR_LEFT);
	} else if (command == "sys") {
#ifdef ESP32
		print("Chip: %s\n", ESP.getChipModel());
		print("Temperature: %.1f °C\n", temperatureRead());
		print("Free heap: %d\n", ESP.getFreeHeap());
		print("Num  Task                Stack  Prio  Core  CPU%%\n");
		int taskCount = uxTaskGetNumberOfTasks();
		TaskStatus_t *systemState = new TaskStatus_t[taskCount];
		uint32_t totalRunTime;
		uxTaskGetSystemState(systemState, taskCount, &totalRunTime);
		for (int i = 0; i < taskCount; i++) {
			String core = systemState[i].xCoreID == tskNO_AFFINITY ? "*" : String(systemState[i].xCoreID);
			int cpuPercentage = systemState[i].ulRunTimeCounter / (totalRunTime / 100);
			print("%-5d%-20s%-7d%-6d%-6s%d\n",systemState[i].xTaskNumber, systemState[i].pcTaskName,
				systemState[i].usStackHighWaterMark, systemState[i].uxCurrentPriority, core, cpuPercentage);
		}
		delete[] systemState;
#endif
	} else if (command == "reset") {
		xSemaphoreTake(mutexState, portMAX_DELAY);
		attitude = Quaternion();
		xSemaphoreGive(mutexState);
	} else if (command == "reboot") {
		ESP.restart();
	} else {
		print("Invalid command: %s\n", command.c_str());
	}
}

/**
 * @brief 处理串口输入
 * 
 * 监听串口数据，当收到换行符时执行命令。
 */
void handleInput() {
	static bool showMotd = true;
	static String input;

	if (showMotd) {
		print("%s\n", motd);
		showMotd = false;
	}

	while (Serial.available()) {
		char c = Serial.read();
		if (c == '\n') {
			doCommand(input);
			input.clear();
		} else {
			input += c;
		}
	}
}