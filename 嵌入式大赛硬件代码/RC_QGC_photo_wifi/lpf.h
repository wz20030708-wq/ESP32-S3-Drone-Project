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

/**
 * @brief 陷波滤波器 (Notch Filter)
 * 用于消除特定频率的振动噪声（如电机振动）。
 * 默认为二阶 IIR 陷波滤波器。
 * 
 * @tparam T 数据类型（通常为 float 或 Vector）
 */
template<typename T>
class NotchFilter {
    public:
        /**
         * @brief 构造函数
         * @param freq 陷波中心频率 (Hz)
         * @param q Q值（带宽 = freq/Q，Q越大带宽越窄）
         * @param fs 采样频率 (Hz)
         */
        NotchFilter(float freq = 0, float q = 5.0f, float fs = 1000.0f) {
            setParams(freq, q, fs);
            output = T();
            input1 = T();
            input2 = T();
            output1 = T();
            output2 = T();
        }
        
        /**
         * @brief 设置滤波器参数
         */
        void setParams(float freq, float q, float fs) {
            if (freq <= 0 || q <= 0) {
                enabled = false;
                return;
            }
            float w0 = 2.0f * PI * freq / fs;
            float alpha = sinf(w0) / (2.0f * q);
            
            b0 = 1.0f;
            b1 = -2.0f * cosf(w0);
            b2 = 1.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cosf(w0);
            a2 = 1.0f - alpha;
            
            // Normalize
            b0 /= a0; b1 /= a0; b2 /= a0;
            a1 /= a0; a2 /= a0;
            
            enabled = true;
        }
        
        /**
         * @brief 更新滤波器
         * @param input 当前输入
         * @return 滤波后的输出
         */
        T update(const T& input) {
            if (!enabled) return input;
            
            output = input * b0 + input1 * b1 + input2 * b2 - output1 * a1 - output2 * a2;
            
            input2 = input1;
            input1 = input;
            output2 = output1;
            output1 = output;
            
            return output;
        }
        
        /** 是否启用 */
        bool enabled = false;
        
    private:
        T output, input1, input2, output1, output2;
        float b0, b1, b2, a0, a1, a2;
};