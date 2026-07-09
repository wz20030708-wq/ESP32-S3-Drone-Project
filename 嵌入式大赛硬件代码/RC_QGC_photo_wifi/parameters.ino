/**
 * @file parameters.ino
 * @brief 参数存储模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2024
 * @version 1.0
 * 
 * 使用ESP32 Preferences API将飞控参数存储在闪存中，支持参数读写和自动同步。
 * 飞行期间暂停闪存写入以避免延迟。
 */

#include <Preferences.h>
#include "util.h"

#include "globals.h"

/** Preferences存储对象 */
Preferences storage;

/**
 * @brief 参数结构体
 */
struct Parameter {
	const char *name;   /**< 参数名称(最大15字符) */
	float *variable;    /**< 参数变量指针 */
	float value;        /**< 缓存值(用于检测变化) */
};

/** 参数列表定义 */
Parameter parameters[] = {
	{"CTL_R_RATE_P", &rollRatePID.p},
	{"CTL_R_RATE_I", &rollRatePID.i},
	{"CTL_R_RATE_D", &rollRatePID.d},
	{"CTL_R_RATE_WU", &rollRatePID.windup},
	{"CTL_P_RATE_P", &pitchRatePID.p},
	{"CTL_P_RATE_I", &pitchRatePID.i},
	{"CTL_P_RATE_D", &pitchRatePID.d},
	{"CTL_P_RATE_WU", &pitchRatePID.windup},
	{"CTL_Y_RATE_P", &yawRatePID.p},
	{"CTL_Y_RATE_I", &yawRatePID.i},
	{"CTL_Y_RATE_D", &yawRatePID.d},
	{"CTL_Y_RATE_WU", &yawRatePID.windup},
	{"CTL_R_P", &rollPID.p},
	{"CTL_R_I", &rollPID.i},
	{"CTL_R_D", &rollPID.d},
	{"CTL_P_P", &pitchPID.p},
	{"CTL_P_I", &pitchPID.i},
	{"CTL_P_D", &pitchPID.d},
	{"CTL_Y_P", &yawPID.p},
	{"CTL_P_RATE_MAX", &maxRate.y},
	{"CTL_R_RATE_MAX", &maxRate.x},
	{"CTL_Y_RATE_MAX", &maxRate.z},
	{"CTL_TILT_MAX", &tiltMax},
	{"IMU_ACC_BIAS_X", &accBias.x},
	{"IMU_ACC_BIAS_Y", &accBias.y},
	{"IMU_ACC_BIAS_Z", &accBias.z},
	{"IMU_ACC_SCALE_X", &accScale.x},
	{"IMU_ACC_SCALE_Y", &accScale.y},
	{"IMU_ACC_SCALE_Z", &accScale.z},
	{"EST_ACC_WEIGHT", &accWeight},
	{"EST_RATES_LPF_A", &ratesFilter.alpha},
	{"RC_ZERO_0", &channelZero[0]},
	{"RC_ZERO_1", &channelZero[1]},
	{"RC_ZERO_2", &channelZero[2]},
	{"RC_ZERO_3", &channelZero[3]},
	{"RC_ZERO_4", &channelZero[4]},
	{"RC_ZERO_5", &channelZero[5]},
	{"RC_ZERO_6", &channelZero[6]},
	{"RC_ZERO_7", &channelZero[7]},
	{"RC_MAX_0", &channelMax[0]},
	{"RC_MAX_1", &channelMax[1]},
	{"RC_MAX_2", &channelMax[2]},
	{"RC_MAX_3", &channelMax[3]},
	{"RC_MAX_4", &channelMax[4]},
	{"RC_MAX_5", &channelMax[5]},
	{"RC_MAX_6", &channelMax[6]},
	{"RC_MAX_7", &channelMax[7]},
	{"RC_ROLL", &rollChannel},
	{"RC_PITCH", &pitchChannel},
	{"RC_THROTTLE", &throttleChannel},
	{"RC_YAW", &yawChannel},
	{"RC_MODE", &modeChannel},
	{"gyro_bias_alpha", nullptr},  // Gyro bias is auto-managed, no storage needed
	{"madgwick_beta", &madgwickBeta},
	{"notch_freq", nullptr},  // Notch filter frequency, set via gyroNotchFilter.setParams()
	{"notch_q", nullptr},     // Notch filter Q factor
	{"hover_perf_log", &hoverPerfLog},
	{"hover_throttle", &hoverThrottle},
};

/**
 * @brief 初始化参数存储
 * 
 * 从闪存读取参数值，若不存在则写入默认值。
 */
void setupParameters() {
	storage.begin("flix", false);
	for (auto &parameter : parameters) {
		if (!parameter.variable) continue;  // Skip read-only parameters
		if (!storage.isKey(parameter.name)) {
			storage.putFloat(parameter.name, *parameter.variable);
		}
		*parameter.variable = storage.getFloat(parameter.name, *parameter.variable);
		parameter.value = *parameter.variable;
	}
}

/**
 * @brief 获取参数总数
 * @return 参数数量
 */
int parametersCount() {
	return sizeof(parameters) / sizeof(parameters[0]);
}

/**
 * @brief 根据索引获取参数名称
 * @param index 参数索引
 * @return 参数名称，索引无效时返回空字符串
 */
const char *getParameterName(int index) {
	if (index < 0 || index >= parametersCount()) return "";
	return parameters[index].name;
}

/**
 * @brief 根据索引获取参数值
 * @param index 参数索引
 * @return 参数值，索引无效时返回NAN
 */
float getParameter(int index) {
	if (index < 0 || index >= parametersCount()) return NAN;
	if (!parameters[index].variable) return NAN;  // Read-only parameter
	return *parameters[index].variable;
}

/**
 * @brief 根据名称获取参数值
 * @param name 参数名称
 * @return 参数值，名称不存在时返回NAN
 */
float getParameter(const char *name) {
	for (auto &parameter : parameters) {
		if (strcmp(parameter.name, name) == 0) {
			if (!parameter.variable) return NAN;  // Read-only parameter
			return *parameter.variable;
		}
	}
	return NAN;
}

/**
 * @brief 根据名称设置参数值
 * @param name 参数名称
 * @param value 参数值
 * @return true表示设置成功，false表示参数不存在
 */
bool setParameter(const char *name, const float value) {
	for (auto &parameter : parameters) {
		if (strcmp(parameter.name, name) == 0) {
			if (!parameter.variable) return false;  // Read-only parameter
			*parameter.variable = value;
			return true;
		}
	}
	return false;
}

/**
 * @brief 同步参数到闪存
 * 
 * 每秒检查一次参数变化，飞行期间暂停同步以避免延迟。
 */
void syncParameters() {
	static Rate rate(1);
	if (!rate) return;
	if (motorsActive()) return;

	for (auto &parameter : parameters) {
		if (!parameter.variable) continue;  // Skip read-only parameters
		if (parameter.value == *parameter.variable) continue;
		if (isnan(parameter.value) && isnan(*parameter.variable)) continue;
		storage.putFloat(parameter.name, *parameter.variable);
		parameter.value = *parameter.variable;
	}
}

/**
 * @brief 打印所有参数
 */
void printParameters() {
	for (auto &parameter : parameters) {
		if (!parameter.variable) continue;  // Skip read-only parameters
		print("%s = %g\n", parameter.name, *parameter.variable);
	}
}

/**
 * @brief 重置所有参数
 * 
 * 清空闪存并重启系统。
 */
void resetParameters() {
	storage.clear();
	ESP.restart();
}