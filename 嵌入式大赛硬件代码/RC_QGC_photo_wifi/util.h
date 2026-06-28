/**
 * @file util.h
 * @brief 通用工具函数集
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 提供飞行控制所需的通用数学工具、字符串处理、速率限制等辅助功能。
 */

#pragma once

#include <math.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>

/** 重力加速度常量 (m/s²) */
const float ONE_G = 9.80665;
/** 全局时间戳(s) */
extern float t;

/**
 * @brief 浮点型数值映射
 * @param x 输入值
 * @param in_min 输入最小值
 * @param in_max 输入最大值
 * @param out_min 输出最小值
 * @param out_max 输出最大值
 * @return 映射后的输出值
 */
float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief 判断浮点数是否无效(NAN或INF)
 * @param x 待判断的浮点数
 * @return true表示无效，false表示有效
 */
bool invalid(float x) {
	return !isfinite(x);
}

/**
 * @brief 判断浮点数是否有效(有限)
 * @param x 待判断的浮点数
 * @return true表示有效，false表示无效
 */
bool valid(float x) {
	return isfinite(x);
}

/**
 * @brief 将角度归一化到[-PI, PI)范围
 * @param angle 原始角度(弧度)
 * @return 归一化后的角度
 */
float wrapAngle(float angle) {
	angle = fmodf(angle, 2 * PI);
	if (angle > PI) {
		angle -= 2 * PI;
	} else if (angle < -PI) {
		angle += 2 * PI;
	}
	return angle;
}

/**
 * @brief 禁用ESP32低电压复位保护
 * 
 * 防止电池电压下降时触发芯片复位，允许无人机继续执行降落程序。
 */
void disableBrownOut() {
	REG_CLR_BIT(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA);
}

/**
 * @brief 按空格分割字符串，最多分成3个部分
 * @param str 输入字符串
 * @param token0 第一个分割结果
 * @param token1 第二个分割结果(可能为空)
 * @param token2 第三个分割结果(可能为空)
 */
void splitString(String& str, String& token0, String& token1, String& token2) {
	str.trim();
	char chars[str.length() + 1];
	str.toCharArray(chars, str.length() + 1);
	token0 = strtok(chars, " ");
	token1 = strtok(NULL, " ");
	token2 = strtok(NULL, "");
}

/**
 * @class Rate
 * @brief 速率限制器
 * 
 * 用于控制某个操作的执行频率，返回值为bool类型，可直接用于条件判断。
 */
class Rate {
public:
	/** 速率(Hz) */
	float rate;
	/** 上次执行时间 */
	float last = 0;

	/**
	 * @brief 构造函数
	 * @param rate 限制速率(Hz)
	 */
	Rate(float rate) : rate(rate) {}

	/**
	 * @brief 判断是否到达执行时间
	 * @return true表示可以执行，false表示还未到时间
	 */
	operator bool() {
		if (t - last >= 1 / rate) {
			last = t;
			return true;
		}
		return false;
	}
};

/**
 * @class Delay
 * @brief 布尔信号延迟滤波器
 * 
 * 确保布尔信号稳定持续指定时间后才返回true，用于防抖处理。
 */
class Delay {
public:
	/** 延迟时间(s) */
	float delay;
	/** 信号开始时间 */
	float start = NAN;

	/**
	 * @brief 构造函数
	 * @param delay 延迟时间(s)
	 */
	Delay(float delay) : delay(delay) {}

	/**
	 * @brief 更新延迟滤波器
	 * @param on 当前信号状态
	 * @return true表示信号已稳定持续指定时间
	 */
	bool update(bool on) {
		if (!on) {
			start = NAN;
			return false;
		} else if (isnan(start)) {
			start = t;
		}
		return t - start >= delay;
	}
};