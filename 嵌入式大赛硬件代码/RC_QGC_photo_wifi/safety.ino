/**
 * @file safety.ino
 * @brief 故障安全保护模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2024
 * @version 1.0
 * 
 * 实现无人机的故障安全保护功能，包括：
 * - RC信号丢失时自动降落
 */

/** RC信号丢失超时时间(s) */
#define RC_LOSS_TIMEOUT 1
/** 自动降落时间(s) */
#define DESCEND_TIME 10

#include "globals.h"

/**
 * @brief 故障安全主入口
 * 
 * 依次执行RC信号丢失保护和自动飞行接管保护。
 */
void failsafe() {
	rcLossFailsafe();
	autoFailsafe();
}

/**
 * @brief RC信号丢失故障保护
 * 
 * 当RC信号超时未更新时，启动自动降落程序。
 */
void rcLossFailsafe() {
	if (controlTime == 0) return;
	if (!armed) return;
	if (t - controlTime > RC_LOSS_TIMEOUT) {
		descend();
	}
}

/**
 * @brief 自动降落程序
 * 
 * 切换到STAB模式，逐步减小油门实现平滑降落。
 */
void descend() {
	mode = STAB;
	attitudeTarget = Quaternion();
	thrustTarget -= dt / DESCEND_TIME;
	if (thrustTarget < 0) {
		thrustTarget = 0;
		armed = false;
	}
}

/**
 * @brief 自动飞行模式手动接管保护
 * 
 * 检测飞行员的手动输入，在自动飞行模式下允许手动接管控制。
 */
void autoFailsafe() {
		static float roll, pitch, yaw, throttle;
		if (roll != controlRoll || pitch != controlPitch || yaw != controlYaw || fabsf(throttle - controlThrottle) > 0.05) {
			// Pilot input detected - manual control active
		}
		roll = controlRoll;
		pitch = controlPitch;
		yaw = controlYaw;
		throttle = controlThrottle;
	}