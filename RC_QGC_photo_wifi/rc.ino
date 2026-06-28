/**
 * @file rc.ino
 * @brief RC遥控接收模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 使用SBUS协议读取RC遥控器数据，并进行通道映射和归一化。
 * 支持自动通道校准，自动识别各控制通道(横滚、俯仰、油门、偏航、模式)。
 */

#include <SBUS.h>
#include "util.h"

/** SBUS接收器对象(RX=14, TX=-1) */
SBUS rc(Serial2, 14, -1);
/** 原始RC通道数据 */
uint16_t channels[16];
/** 上次控制更新时间 */
float controlTime;
/** 通道校准零点 */
float channelZero[16];
/** 通道校准最大值 */
float channelMax[16];

/** 各控制通道映射 */
float rollChannel = NAN, pitchChannel = NAN, throttleChannel = NAN, yawChannel = NAN, modeChannel = NAN;

/**
 * @brief 初始化RC接收器
 */
void setupRC() {
	print("Setup RC\n");
	rc.begin();
}

/**
 * @brief 读取RC数据并归一化
 * @return true表示成功读取，false表示无数据
 */
bool readRC() {
	if (rc.read()) {
		SBUSData data = rc.data();
		for (int i = 0; i < 16; i++) channels[i] = data.ch[i];
		normalizeRC();
		controlTime = t;
		return true;
	}
	return false;
}

/**
 * @brief 将原始RC通道数据归一化到0~1范围
 */
void normalizeRC() {
	float controls[16];
	for (int i = 0; i < 16; i++) {
		controls[i] = mapf(channels[i], channelZero[i], channelMax[i], 0, 1);
	}
	controlRoll = rollChannel >= 0 ? controls[(int)rollChannel] : NAN;
	controlPitch = pitchChannel >= 0 ? controls[(int)pitchChannel] : NAN;
	controlYaw = yawChannel >= 0 ? controls[(int)yawChannel] : NAN;
	controlThrottle = throttleChannel >= 0 ? controls[(int)throttleChannel] : NAN;
	controlMode = modeChannel >= 0 ? controls[(int)modeChannel] : NAN;
}

/**
 * @brief RC通道自动校准
 * 
 * 通过8个步骤依次识别各控制通道的映射关系。
 */
void calibrateRC() {
	uint16_t zero[16];
	uint16_t center[16];
	uint16_t max[16];
	print("1/8 校准遥控,所有摇杆归中位置[3秒]\n");
	pause(8);
	calibrateRCChannel(NULL, zero, zero, "2/8 左摇杆:向下,右摇杆:居中[3秒]\n...     ...\n...     .o.\n.o.     ...\n");
	calibrateRCChannel(NULL, center, center, "3/8 左摇杆:居中,右摇杆:居中[3秒]\n...     ...\n.o.     .o.\n...     ...\n");
	calibrateRCChannel(&throttleChannel, zero, max, "4/8 油门通道识别,左摇杆:向上推到底(油门最大),右摇杆：居中[3秒]\n.o.     ...\n...     .o.\n...     ...\n");
	calibrateRCChannel(&yawChannel, center, max, "5/8 偏航通道识别,左摇杆:向右推到底(偏航右转)右摇杆:居中[3秒]\n...     ...\n..o     .o.\n...     ...\n");
	calibrateRCChannel(&pitchChannel, zero, max, "6/8 俯仰通道识别,左摇杆:向下推到底,右摇杆:向上推到底(俯仰前进)[3秒]\n...     .o.\n...     ...\n.o.     ...\n");
	calibrateRCChannel(&rollChannel, zero, max, "7/8 横滚通道识别,左摇杆:向下推到底,右摇杆:向右推到底(横滚右转)[3秒]\n...     ...\n...     ..o\n.o.     ...\n");
	calibrateRCChannel(&modeChannel, zero, max, "8/8 模式通道识别,先将解锁开关拨回锁定位置,然后将模式开关拨到最高档位(如手动模式)[3秒]\n");
	printRCCalibration();
}

/**
 * @brief 单个RC通道校准采样
 * 
 * 对比两次采样数据，找出变化最大的通道作为目标通道。
 * 
 * @param channel 待校准的通道指针(为NULL时仅更新数据)
 * @param in 上次采样数据
 * @param out 当前采样数据
 * @param str 提示信息
 */
void calibrateRCChannel(float *channel, uint16_t in[16], uint16_t out[16], const char *str) {
	print("%s", str);
	pause(8);
	for (int i = 0; i < 30; i++) readRC();
	memcpy(out, channels, sizeof(channels));

	if (channel == NULL) return;

	int ch = -1, diff = 0;
	for (int i = 0; i < 16; i++) {
		if (abs(out[i] - in[i]) > diff) {
			ch = i;
			diff = abs(out[i] - in[i]);
		}
	}
	if (ch >= 0 && diff > 10) {
		*channel = ch;
		channelZero[ch] = in[ch];
		channelMax[ch] = out[ch];
	} else {
		*channel = NAN;
	}
}

/**
 * @brief 打印RC校准结果
 */
void printRCCalibration() {
	print("Control   Ch     Zero   Max\n");
	print("Roll      %-7g%-7g%-7g\n", rollChannel, rollChannel >= 0 ? channelZero[(int)rollChannel] : NAN, rollChannel >= 0 ? channelMax[(int)rollChannel] : NAN);
	print("Pitch     %-7g%-7g%-7g\n", pitchChannel, pitchChannel >= 0 ? channelZero[(int)pitchChannel] : NAN, pitchChannel >= 0 ? channelMax[(int)pitchChannel] : NAN);
	print("Yaw       %-7g%-7g%-7g\n", yawChannel, yawChannel >= 0 ? channelZero[(int)yawChannel] : NAN, yawChannel >= 0 ? channelMax[(int)yawChannel] : NAN);
	print("Throttle  %-7g%-7g%-7g\n", throttleChannel, throttleChannel >= 0 ? channelZero[(int)throttleChannel] : NAN, throttleChannel >= 0 ? channelMax[(int)throttleChannel] : NAN);
	print("Mode      %-7g%-7g%-7g\n", modeChannel, modeChannel >= 0 ? channelZero[(int)modeChannel] : NAN, modeChannel >= 0 ? channelMax[(int)modeChannel] : NAN);
}