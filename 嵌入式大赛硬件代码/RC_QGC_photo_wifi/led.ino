/**
 * @file led.ino
 * @brief 板载LED灯控制模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 根据系统状态控制板载LED指示灯，提供视觉反馈：
 * - 解锁状态：常亮
 * - 已连接(MAVLink/RC)：慢闪
 * - 未连接：快闪
 */

/** 慢闪间隔(微秒) - 0.5Hz */
#define BLINK_SLOW  1000000
/** 快闪间隔(微秒) - 2.5Hz */
#define BLINK_FAST  200000

#ifndef LED_BUILTIN
/** LED指示灯引脚 */
#define LED_BUILTIN 48
#endif

#include "globals.h"

/**
 * @brief 初始化LED引脚
 */
void setupLED() {
	pinMode(LED_BUILTIN, OUTPUT);
}

/**
 * @brief 设置LED状态
 * @param on true为点亮，false为熄灭
 */
void setLED(bool on) {
	static bool state = false;
	if (on == state) {
		return;
	}
	digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
	state = on;
}

/**
 * @brief LED闪烁控制
 * 
 * 根据系统状态自动切换LED闪烁模式：
 * - 解锁后常亮
 * - 已连接时慢闪
 * - 未连接时快闪
 */
void blinkLED() {
	if (armed) {
		setLED(true);
		return;
	}

	bool mavActive = mavlinkConnected && (t - lastMavlinkTime < 2.0f);
	bool rcActive = controlTime > 0 && (t - controlTime < 1.0f);
	if (mavActive || rcActive) {
		setLED(micros() / BLINK_SLOW % 2);
	} else {
		setLED(micros() / BLINK_FAST % 2);
	}
}