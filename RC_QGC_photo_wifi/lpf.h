/**
 * @file lpf.h
 * @brief 低通滤波器实现
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 提供通用的一阶低通滤波器模板类，支持标量和向量类型的数据滤波。
 * 滤波公式: output = output + alpha * (input - output)
 * alpha越接近1，滤波效果越弱，响应越快；alpha越接近0，滤波效果越强，响应越慢。
 */

#pragma once

/**
 * @class LowPassFilter
 * @brief 一阶低通滤波器模板类
 * @tparam T 滤波数据类型，支持float、Vector等
 */
template <typename T>
class LowPassFilter {
public:
	/** 平滑系数，范围[0,1]，1表示禁用滤波 */
	float alpha;
	/** 滤波器输出值 */
	T output;

	/**
	 * @brief 构造函数
	 * @param alpha 平滑系数
	 */
	LowPassFilter(float alpha): alpha(alpha) {};

	/**
	 * @brief 更新滤波器，输入新数据并返回滤波后的值
	 * @param input 新输入数据
	 * @return 滤波后的输出值
	 */
	T update(const T input) {
		if (alpha == 1) {
			return input;
		}

		if (!initialized) {
			output = input;
			initialized = true;
		}

		return output += alpha * (input - output);
	}

	/**
	 * @brief 根据截止频率和采样周期计算alpha值
	 * @param cutOffFreq 截止频率(Hz)
	 * @param dt 采样周期(s)
	 */
	void setCutOffFrequency(float cutOffFreq, float dt) {
		alpha = 1 - exp(-2 * PI * cutOffFreq * dt);
	}

	/**
	 * @brief 重置滤波器状态
	 */
	void reset() {
		initialized = false;
	}

private:
	/** 滤波器初始化标志 */
	bool initialized = false;
};