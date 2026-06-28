/**
 * @file pid.h
 * @brief PID控制器实现
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 实现标准的增量式PID控制器，支持积分饱和限制和微分项低通滤波。
 * PID输出 = Kp * error + Ki * integral + Kd * derivative
 */

#pragma once

#include "lpf.h"

/**
 * @class PID
 * @brief PID控制器类
 * 
 * 实现标准PID控制算法，包含：
 * - 比例项(P): 响应当前误差
 * - 积分项(I): 消除稳态误差，带积分饱和限制
 * - 微分项(D): 抑制振荡，带低通滤波
 */
class PID {
public:
	/** 比例系数 */
	float p;
	/** 积分系数 */
	float i;
	/** 微分系数 */
	float d;
	/** 积分饱和限制 */
	float windup;
	/** 最大允许时间间隔(s)，超过则重置积分和微分 */
	float dtMax;

	/** 微分项当前值 */
	float derivative = 0;
	/** 积分项当前值 */
	float integral = 0;

	/** 微分项低通滤波器 */
	LowPassFilter<float> lpf;

	/**
	 * @brief 构造函数
	 * @param p 比例系数
	 * @param i 积分系数
	 * @param d 微分系数
	 * @param windup 积分饱和限制(默认0，无限制)
	 * @param dAlpha 微分低通滤波器平滑系数(默认1，禁用滤波)
	 * @param dtMax 最大时间间隔(默认0.1s)
	 */
	PID(float p, float i, float d, float windup = 0, float dAlpha = 1, float dtMax = 0.1) :
		p(p), i(i), d(d), windup(windup), lpf(dAlpha), dtMax(dtMax) {}

	/**
	 * @brief 更新PID控制器
	 * @param error 当前误差值
	 * @return PID输出值
	 */
	float update(float error) {
		float dt = t - prevTime;

		if (dt > 0 && dt < dtMax) {
			integral += error * dt;
			derivative = lpf.update((error - prevError) / dt);
		} else {
			integral = 0;
			derivative = 0;
		}

		prevError = error;
		prevTime = t;

		return p * error + constrain(i * integral, -windup, windup) + d * derivative;
	}

	/**
	 * @brief 重置PID控制器状态
	 */
	void reset() {
		prevError = NAN;
		prevTime = NAN;
		integral = 0;
		derivative = 0;
		lpf.reset();
	}

private:
	/** 上一时刻误差 */
	float prevError = NAN;
	/** 上一时刻时间戳 */
	float prevTime = NAN;
};