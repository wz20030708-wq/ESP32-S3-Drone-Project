/**
 * @file voltage.ino
 * @brief ADC电压测量模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 通过ESP32-S3内置ADC测量电机电源电压和芯片供电电压。
 * 使用1:1分压电路，采样后乘以2恢复实际电压。
 * 
 * 硬件连接:
 *   GPIO1 (ADC1_CH0) → 电机电源分压点
 *   GPIO2 (ADC1_CH1) → 芯片供电分压点
 */

#include "lpf.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ====================== ADC 引脚定义 ======================

/** 电机电源电压测量引脚 */
#define ADC_PIN_MOTOR   1
/** 芯片供电电压测量引脚 */
#define ADC_PIN_CHIP    2

// ====================== 分压电路补偿系数 ======================

/** 分压比(1:1分压 → ×2恢复) */
#define DIVIDER_RATIO   2.0f

// ====================== ADC 校准系数 ======================

/** 电机电压通道校准系数 */
#define ADC_CAL_MOTOR   1.0f
/** 芯片电压通道校准系数 */
#define ADC_CAL_CHIP    1.0f

// ====================== 滤波参数 ======================

/** EMA平滑系数 */
#define EMA_ALPHA       0.05f

// ====================== 全局变量 ======================

/** 滤波后的电机电源电压(V) */
float motorVoltage = 0.0f;
/** 滤波后的芯片供电电压(V) */
float chipVoltage  = 0.0f;

/** 电机电压原始ADC读数(0-4095) */
uint16_t motorAdcRaw = 0;
/** 芯片电压原始ADC读数(0-4095) */
uint16_t chipAdcRaw  = 0;

/** 电机电压低通滤波器 */
static LowPassFilter<float> motorFilter(EMA_ALPHA);
/** 芯片电压低通滤波器 */
static LowPassFilter<float> chipFilter(EMA_ALPHA);

// ====================== 初始化函数 ======================

/**
 * @brief 初始化ADC电压测量模块
 */
void setupADC() {
		print("设置ADC电压测量\n");

		analogReadResolution(12);
		analogSetAttenuation(ADC_11db);

		for (int i = 0; i < 8; i++) {
			analogRead(ADC_PIN_MOTOR);
			analogRead(ADC_PIN_CHIP);
		}

		motorAdcRaw = analogRead(ADC_PIN_MOTOR);
		chipAdcRaw  = analogRead(ADC_PIN_CHIP);

		motorVoltage = adcToVoltage(motorAdcRaw, ADC_CAL_MOTOR);
		chipVoltage  = adcToVoltage(chipAdcRaw,  ADC_CAL_CHIP);

		print("ADC 电压测量初始化完成\n");
		print("电机电压引脚: GPIO%d, 芯片电压引脚: GPIO%d\n", ADC_PIN_MOTOR, ADC_PIN_CHIP);
	}

	// ====================== ADC 转换函数 ======================

/**
 * @brief 将ADC原始值转换为实际电压
 * @param raw ADC原始读数(0-4095)
 * @param calFactor 校准系数
 * @return 实际电压(V)
 */
float adcToVoltage(uint16_t raw, float calFactor) {
	float v_adc = (raw / 4095.0f) * 3.3f;
	return v_adc * DIVIDER_RATIO * calFactor;
}

// ====================== 电压读取与滤波 ======================

/**
 * @brief 读取两路ADC电压并应用低通滤波
 */
void readVoltages() {
	motorAdcRaw = analogRead(ADC_PIN_MOTOR);
	float motorMeasured = motorAdcRaw / 4095.0f * 3.3f;
	float motorFiltered = motorFilter.update(motorMeasured);
	motorVoltage = motorFiltered * DIVIDER_RATIO * ADC_CAL_MOTOR;

	if (motorVoltage < 0.0f) motorVoltage = 0.0f;
	if (motorVoltage > 8.0f) motorVoltage = 8.0f;

	chipAdcRaw = analogRead(ADC_PIN_CHIP);
	float chipMeasured = chipAdcRaw / 4095.0f * 3.3f;
	float chipFiltered = chipFilter.update(chipMeasured);
	chipVoltage = chipFiltered * DIVIDER_RATIO * ADC_CAL_CHIP;

	if (chipVoltage < 0.0f) chipVoltage = 0.0f;
	if (chipVoltage > 8.0f) chipVoltage = 8.0f;
}

// ====================== 调试输出 ======================

/**
 * @brief 打印电压测量数据(调试用)
 */
void printVoltages() {
	print("ADC 电机: %4d raw → %.2f V | 芯片: %4d raw → %.2f V\n",
		motorAdcRaw, motorVoltage, chipAdcRaw, chipVoltage);
}