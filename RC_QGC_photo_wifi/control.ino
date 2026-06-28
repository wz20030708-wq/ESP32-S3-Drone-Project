/**
 * @file control.ino
 * @brief 飞行控制模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 实现四旋翼无人机的姿态控制和电机混控。
 * 采用串级PID控制结构：角度环(外环)→角速度环(内环)→电机输出。
 * 支持多种飞行模式：RAW(直接力矩控制)、ACRO(姿态率控制)、STAB(姿态稳定)、OBSTACLE(避障)。
 */

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "lpf.h"
#include "util.h"

// ============== 角速率环（内环）参数 ==============
#define PITCHRATE_P 0.109
#define PITCHRATE_I 0.03
#define PITCHRATE_D 0.009
#define PITCHRATE_I_LIM 0.35
#define ROLLRATE_P 0.055
#define ROLLRATE_I 0.2 
#define ROLLRATE_D 0.007 
#define ROLLRATE_I_LIM 0.35
#define YAWRATE_P 0.4
#define YAWRATE_I 0.01
#define YAWRATE_D 0.01
#define YAWRATE_I_LIM 0.3

// ============== 角度环（外环）参数 ==============
#define ROLL_P 7
#define ROLL_I 0
#define ROLL_D 0
#define PITCH_P ROLL_P
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D
#define YAW_P 3

// ============== 限制值 ==============
#define PITCHRATE_MAX radians(360)
#define ROLLRATE_MAX radians(360)
#define YAWRATE_MAX radians(300)
#define TILT_MAX radians(30)
#define RATES_D_LPF_ALPHA 0.2
#define YAWRATE_D_LPF_ALPHA 0.2

/** 飞行模式枚举 */
const int RAW = 0, ACRO = 1, STAB = 2, AUTO = 3, OBSTACLE = 4;
/** 当前飞行模式 */
int mode = STAB;
/** 解锁状态 */
bool armed = false;

/** 避障安全距离(mm) */
float safeDistance = 500.0f;

/** 横滚速率PID控制器 */
PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
/** 俯仰速率PID控制器 */
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
/** 偏航速率PID控制器 */
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D, YAWRATE_I_LIM, YAWRATE_D_LPF_ALPHA);
/** 横滚角度PID控制器 */
PID rollPID(ROLL_P, ROLL_I, ROLL_D);
/** 俯仰角度PID控制器 */
PID pitchPID(PITCH_P, PITCH_I, PITCH_D);
/** 偏航角度PID控制器 */
PID yawPID(YAW_P, 0, 0);

/** 最大角速度限制 */
Vector maxRate(ROLLRATE_MAX, PITCHRATE_MAX, YAWRATE_MAX);
/** 最大倾斜角限制 */
float tiltMax = TILT_MAX;

/** 目标姿态四元数 */
Quaternion attitudeTarget;
/** 目标角速度 */
Vector ratesTarget;
/** 前馈角速度 */
Vector ratesExtra;
/** 目标力矩 */
Vector torqueTarget;
/** 目标推力 */
float thrustTarget;

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;
extern float distanceMm;

/**
 * @brief 飞行控制主入口
 * 
 * 依次执行：控制解析→故障安全→姿态控制→速率控制→力矩控制→电机输出
 */
void control() {
	interpretControls();
	failsafe();
	controlAttitude();
	controlRates();
	controlTorque();
}

/**
 * @brief 解析遥控输入，设置目标状态
 * 
 * 根据飞行模式和RC输入，计算目标姿态、角速度和推力。
 * OBSTACLE模式下还会处理避障逻辑。
 */
void interpretControls() {
	if (!isnan(controlMode)) {
		mode = controlMode < 0.5 ? STAB : OBSTACLE;
	}

	if (mode != STAB && mode != OBSTACLE) mode = STAB;

	if (mode == AUTO) return;

	if (controlThrottle < 0.05 && controlYaw > 0.95) armed = true;
	if (controlThrottle < 0.05 && controlYaw < -0.95) armed = false;

	if (abs(controlYaw) < 0.1) controlYaw = 0;

	thrustTarget = controlThrottle;

	if (mode == STAB || mode == OBSTACLE) {
		if (mode == OBSTACLE && distanceMm > 0 && distanceMm <= safeDistance) {
			controlPitch = -mapf(constrain(distanceMm, 30, safeDistance), 30, safeDistance, 0.8, 0.2);
		}

		float yawTarget = attitudeTarget.getYaw();
		if (!armed || invalid(yawTarget) || controlYaw != 0) yawTarget = attitude.getYaw();
		attitudeTarget = Quaternion::fromEuler(Vector(controlRoll * tiltMax, controlPitch * tiltMax, yawTarget));
		ratesExtra = Vector(0, 0, -controlYaw * maxRate.z);
	}

	if (mode == ACRO) {
		attitudeTarget.invalidate();
		ratesTarget.x = controlRoll * maxRate.x;
		ratesTarget.y = controlPitch * maxRate.y;
		ratesTarget.z = -controlYaw * maxRate.z;
	}

	if (mode == RAW) {
		attitudeTarget.invalidate();
		ratesTarget.invalidate();
		torqueTarget = Vector(controlRoll, controlPitch, -controlYaw) * 0.1;
	}
}

/**
 * @brief 姿态角度控制(外环)
 * 
 * 根据目标姿态和当前姿态的偏差，计算目标角速度。
 */
void controlAttitude() {
	if (!armed || attitudeTarget.invalid() || thrustTarget < 0.1) return;

	const Vector up(0, 0, 1);
	Vector upActual = Quaternion::rotateVector(up, attitude);
	Vector upTarget = Quaternion::rotateVector(up, attitudeTarget);

	Vector error = Vector::rotationVectorBetween(upTarget, upActual);

	ratesTarget.x = rollPID.update(error.x) + ratesExtra.x;
	ratesTarget.y = pitchPID.update(error.y) + ratesExtra.y;

	float yawError = wrapAngle(radians(attitudeTarget.getYaw() - attitude.getYaw()));
	ratesTarget.z = yawPID.update(yawError) + ratesExtra.z;
}

/**
 * @brief 角速度控制(内环)
 * 
 * 根据目标角速度和当前角速度的偏差，计算目标力矩。
 */
void controlRates() {
	if (!armed || ratesTarget.invalid() || thrustTarget < 0.1) return;

	Vector error = ratesTarget - rates;

	torqueTarget.x = rollRatePID.update(error.x);
	torqueTarget.y = pitchRatePID.update(error.y);
	torqueTarget.z = yawRatePID.update(error.z);
}

/**
 * @brief 力矩控制与电机混控
 * 
 * 根据目标力矩和推力，计算四个电机的输出值。
 * 四旋翼电机编号(俯视逆时针):
 *   0: 后左(RL)  1: 后右(RR)  2: 前右(FR)  3: 前左(FL)
 */
void controlTorque() {
	if (!torqueTarget.valid()) return;

	if (!armed) {
		memset(motors, 0, sizeof(motors));
		return;
	}

	if (thrustTarget < 0.1) {
		memset(motors, 0, sizeof(motors));
		return;
	}

	motors[MOTOR_FRONT_LEFT] = thrustTarget + torqueTarget.x - torqueTarget.y + torqueTarget.z;
	motors[MOTOR_FRONT_RIGHT] = thrustTarget - torqueTarget.x - torqueTarget.y - torqueTarget.z;
	motors[MOTOR_REAR_LEFT] = thrustTarget + torqueTarget.x + torqueTarget.y - torqueTarget.z;
	motors[MOTOR_REAR_RIGHT] = thrustTarget - torqueTarget.x + torqueTarget.y + torqueTarget.z;

	motors[0] = constrain(motors[0], 0, 1);
	motors[1] = constrain(motors[1], 0, 1);
	motors[2] = constrain(motors[2], 0, 1);
	motors[3] = constrain(motors[3], 0, 1);
}

/**
 * @brief 获取当前飞行模式名称
 * @return 模式名称字符串
 */
const char* getModeName() {
	switch (mode) {
		case RAW: return "RAW";
		case ACRO: return "ACRO";
		case STAB: return "STAB";
		case AUTO: return "AUTO";
		case OBSTACLE: return "OBSTACLE";
		default: return "UNKNOWN";
	}
}