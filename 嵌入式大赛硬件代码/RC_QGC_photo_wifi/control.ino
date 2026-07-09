/**
 * @file control.ino
 * @brief 飞行控制模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 2.0
 *
 * 实现四旋翼无人机的姿态控制和电机混控。
 * 采用 4 层级联 PID 控制结构（参考 Crazyflie / ESP-Drone）：
 *   位置环 → 速度环 → 姿态环(角度) → 角速度环 → 电机输出
 *
 * 飞行模式：
 *   - STAB：手动姿态稳定模式，方向摇杆直接控制目标姿态
 *   - HOVER (OBSTACLE)：悬停锁定模式，禁用方向摇杆，由速度环+
 *                       姿态环自动维持水平静止，气压计维持定高
 */

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "lpf.h"
#include "util.h"
#include "hover_controller.h"

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
#define ROLL_I 0.001
#define ROLL_I_LIM 0.05
#define ROLL_D 0
#define PITCH_P ROLL_P
#define PITCH_I ROLL_I
#define PITCH_I_LIM 0.05
#define PITCH_D ROLL_D
#define YAW_P 3

// ============== 悬停模式专用参数（大疆 P 模式风格）==============
// 速度环 PID（参考 Crazyflie / PX4 velocity PID 量级）
// 输入：水平速度误差 (m/s)，输出：目标倾角 (rad)
#define HOVER_VEL_KP  0.4f    // P: 速度误差 → 倾角，0.4 rad/(m/s)
#define HOVER_VEL_KI  0.15f   // I: 消除稳态漂移（重心偏移/电机不对称）
#define HOVER_VEL_KD  0.05f   // D: 抑制速度振荡
#define HOVER_VEL_ILIM 0.1f   // 积分饱和限幅 (rad)
#define HOVER_TILT_MAX radians(12)  // 最大补偿倾角
#define HOVER_VEL_TAU  1.5f    // 速度估计衰减时间常数 (s)，增大以让速度估计保持更久

// 大疆风格操作参数
#define HOVER_MAX_HORIZ_VEL  1.5f   // 最大水平速度 (m/s)，方向摇杆满杆对应此速度
#define HOVER_MAX_VERT_VEL   1.0f   // 最大垂直速度 (m/s)，油门杆满杆对应此速度
#define HOVER_STICK_DEADBAND 0.05f  // 摇杆居中死区

// ============== 限制值 ==============
#define PITCHRATE_MAX radians(360)
#define ROLLRATE_MAX radians(360)
#define YAWRATE_MAX radians(300)
#define TILT_MAX radians(30)
#define RATES_D_LPF_ALPHA 0.2
#define YAWRATE_D_LPF_ALPHA 0.2

// ============== 低油门积分保护（参考 dRonin lowThrottleZeroIntegral）==============
// 油门低于此阈值时清零所有 PID 积分项，防止地面/怠速时积分累积导致起飞跳变
#define LOW_THROTTLE_INTEGRAL_CLEAR 0.12f

// ============== RC 输入平滑滤波器（参考 dRonin smoothcontrol）==============
// 消除摇杆抖动和突变，提升操控手感。只平滑 roll/pitch/yaw，不平滑油门。
#define RC_SMOOTH_ALPHA 0.3f
static LowPassFilter<float> rcRollSmooth(RC_SMOOTH_ALPHA);
static LowPassFilter<float> rcPitchSmooth(RC_SMOOTH_ALPHA);
static LowPassFilter<float> rcYawSmooth(RC_SMOOTH_ALPHA);

/** 飞行模式枚举 */
const int STAB = 0, OBSTACLE = 1;
/** 当前飞行模式 */
int mode = STAB;
/** 解锁状态 */
bool armed = false;


/** 横滚速率PID控制器 */
PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
/** 俯仰速率PID控制器 */
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
/** 偏航速率PID控制器 */
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D, YAWRATE_I_LIM, YAWRATE_D_LPF_ALPHA);
/** 横滚角度PID控制器 */
PID rollPID(ROLL_P, ROLL_I, ROLL_D, ROLL_I_LIM);
/** 俯仰角度PID控制器 */
PID pitchPID(PITCH_P, PITCH_I, PITCH_D, PITCH_I_LIM);
/** 偏航角度PID控制器 */
PID yawPID(YAW_P, 0, 0);
/** 高度位置环PID (外环: 高度误差→目标垂直速度) */
PID altPosPID(0.8, 0.05, 0.0, 0.2);  // 增加 I 项消除稳态高度偏差
/** 垂直速度环PID (内环: 速度误差→油门偏置) */
PID altVelPID(1.0, 0.15, 0.05, 0.3);  // 降 P 增益防振荡

/** 专业悬停控制器（速度环级联） */
HoverController hoverCtrl;

/** 最大角速度限制 */
Vector maxRate(ROLLRATE_MAX, PITCHRATE_MAX, YAWRATE_MAX);
/** 最大倾斜角限制 */
float tiltMax = TILT_MAX;

/** 目标姿态四元数 */
Quaternion attitudeTarget;
/** 目标角速度 */
Vector ratesTarget;
/** 目标力矩 */
Vector torqueTarget;
/** 目标推力 */
float thrustTarget;

#include "globals.h"

/** 悬停性能监控：是否启用日志 (0.0=关闭, 1.0=开启) */
float hoverPerfLog = 0.0f;
/** 上次性能日志输出时间 */
static float lastHoverLogTime = 0;
/** 悬停油门基准值 */
float hoverThrottle = 0.45f;
/** 定高目标高度 */
float altitudeTarget = 0;
/** 悬停控制器初始化标志 */
static bool hoverCtrlInitialized = false;

/**
 * @brief 初始化悬停控制器参数
 */
static void initHoverController() {
	hoverCtrl.begin(
		HOVER_VEL_KP,      // 速度环 P
		HOVER_VEL_KI,      // 速度环 I
		HOVER_VEL_KD,      // 速度环 D
		HOVER_VEL_ILIM,    // 速度环积分饱和
		HOVER_TILT_MAX,    // 最大倾角
		HOVER_VEL_TAU,     // 速度估计衰减时间常数
		HOVER_MAX_HORIZ_VEL  // 最大水平速度
	);
	hoverCtrlInitialized = true;
}

/**
 * @brief 悬停性能监控日志
 * 每100ms输出姿态、速度估计和目标速度，用于调试和优化悬停控制。
 */
void logHoverPerformance() {
    if (!hoverPerfLog) return;
    if (!armed || thrustTarget < 0.1f) return;
    if (mode != OBSTACLE) return;  // 仅在悬停模式下输出
    if (t - lastHoverLogTime < 0.1f) return;
    lastHoverLogTime = t;

    Vector currentEuler = attitude.toEuler();
    Vector velEst = hoverCtrl.getVelocityEstimate();

    // 计算当前目标速度（用于调试）
    float pitchStick = applyDeadband(controlPitch, HOVER_STICK_DEADBAND);
    float rollStick  = applyDeadband(controlRoll,  HOVER_STICK_DEADBAND);
    Vector bodyVelTarget(pitchStick * HOVER_MAX_HORIZ_VEL,
						 rollStick  * HOVER_MAX_HORIZ_VEL, 0);
    Vector worldVelTarget = Quaternion::rotateVector(bodyVelTarget, attitude);

    float velXErr, velYErr, velXInt, velYInt;
    hoverCtrl.getPIDState(velXErr, velYErr, velXInt, velYInt);

    print("HOVER: attitude[r=%.2f p=%.2f y=%.2f] | vel[tgtX=%.2f estX=%.2f tgtY=%.2f estY=%.2f] | "
		  "pidInt[X=%.3f Y=%.3f] | alt=%.1fm thrust=%.2f\n",
        degrees(currentEuler.x), degrees(currentEuler.y), degrees(currentEuler.z),
        worldVelTarget.x, velEst.x, worldVelTarget.y, velEst.y,
        velXInt, velYInt,
        altFiltered, thrustTarget);
}

/**
 * @brief 平滑死区处理
 * 死区内输出 0，死区外线性映射到 [-1, 1]，边界连续。
 * @param value 原始输入值（范围 -1~1）
 * @param deadband 死区半宽
 * @return 处理后的值，死区内为 0，死区外线性放大
 */
static float applyDeadband(float value, float deadband) {
	if (fabsf(value) <= deadband) return 0;
	float sign = (value > 0) ? 1.0f : -1.0f;
	return sign * (fabsf(value) - deadband) / (1.0f - deadband);
}

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
	logHoverPerformance();
}

/**
 * @brief 解析遥控输入，设置目标状态
 *
 * 根据飞行模式和RC输入，计算目标姿态、角速度和推力。
 */
void interpretControls() {
	if (!hoverCtrlInitialized) initHoverController();

	if (!isnan(controlMode)) {
		static int debounceCount = 0;
		static int lastDebounceMode = -1;
		int targetMode = mode;

		// Hysteresis: STAB→OBSTACLE at 0.7, OBSTACLE→STAB at 0.3
		if (mode == STAB && controlMode >= 0.7f) {
			targetMode = OBSTACLE;
		} else if (mode == OBSTACLE && controlMode <= 0.3f) {
			targetMode = STAB;
		}

		// Debounce: require 3 consecutive samples
		if (targetMode != lastDebounceMode) {
			debounceCount = 0;
			lastDebounceMode = targetMode;
		} else if (targetMode != mode) {
			debounceCount++;
			if (debounceCount >= 3) {
				int prevMode = mode;
				mode = targetMode;
				if (mode == OBSTACLE) {
					// 进入悬停模式：重置所有悬停相关状态
					altitudeTarget = altFiltered;
					altVelPID.reset();
					altPosPID.reset();
					hoverCtrl.reset();
				}
				debounceCount = 0;
				print("Mode switched to: %s\n", getModeName());
			}
		}
	}

	if (controlThrottle < 0.05 && controlYaw > 0.95) armed = true;
	if (controlThrottle < 0.05 && controlYaw < -0.95) armed = false;

	// RC 输入平滑（参考 dRonin smoothcontrol）
	// 仅平滑 roll/pitch/yaw，消除摇杆抖动和突变；油门不平滑以保持精确控制。
	// 注意：解锁/锁定判断使用原始 controlYaw（上方），不受平滑影响，保证安全响应。
	// 平滑在死区处理之前，使死区在已平滑信号上工作，避免抖动穿越死区边界。
	controlRoll = rcRollSmooth.update(controlRoll);
	controlPitch = rcPitchSmooth.update(controlPitch);
	controlYaw = rcYawSmooth.update(controlYaw);

	if (abs(controlYaw) < 0.1) controlYaw = 0;

	thrustTarget = controlThrottle;

	if (mode == OBSTACLE) {
		// ============================================================
		// 悬停模式 (HOVER) — 大疆 P 模式风格
		// ============================================================
		// 操作逻辑（仿大疆）：
		//   - 方向摇杆(roll/pitch) = 水平速度指令（居中=悬停，偏离=移动）
		//   - 油门杆 = 垂直速度指令（居中=定高，偏离=升降）
		//   - 偏航摇杆 = 偏航角速率（居中=锁定航向）
		//   - 松杆 = 自动悬停
		//
		// 控制架构：
		//   1. 水平环：摇杆→目标速度→速度环PID→目标倾角
		//   2. 垂直环：油门杆→目标垂直速度/定高→速度环PID→油门
		//   3. 偏航：摇杆→偏航角速率
		// ============================================================

		// ----- 1. 水平速度控制（大疆风格）-----
		// 摇杆输入 → 机体系目标速度 → 旋转到世界系
		// 使用平滑死区，避免边界跳变
		float pitchStick = applyDeadband(controlPitch, HOVER_STICK_DEADBAND);
		float rollStick  = applyDeadband(controlRoll, HOVER_STICK_DEADBAND);

		// 机体系速度目标：前推=前进(+X)，右推=右移(+Y)
		Vector bodyVelTarget(pitchStick * HOVER_MAX_HORIZ_VEL,
							 rollStick  * HOVER_MAX_HORIZ_VEL, 0);
		// 旋转到世界系（无论航向如何，"前进"始终是无人机机头方向）
		Vector worldVelTarget = Quaternion::rotateVector(bodyVelTarget, attitude);

		// 调用悬停控制器：速度环 PID 输出目标倾角
		// 传入 landed 状态以启用加速度零偏估计
		Vector tiltCmd = hoverCtrl.update(acc, attitude, dt,
										  worldVelTarget.x, worldVelTarget.y, landed);

		// ----- 2. 垂直速度/定高控制（大疆风格）-----
		// 油门杆居中(死区内) → 定高；偏离 → 控制垂直速度
		// 使用平滑死区：throttleCmd 已归一化到 [-1, 1]，0 表示定高，非 0 表示垂直速度指令
		const float ALT_CORRECTION_MAX = 0.25f;
		float throttleCentered = controlThrottle - 0.5f;
		float throttleCmd = applyDeadband(throttleCentered, HOVER_STICK_DEADBAND);
		// throttleCmd: 0 表示定高，非 0 表示垂直速度指令
		bool isAltHold = (throttleCmd == 0);

		if (altFiltered > 0) {
			if (isAltHold) {
				// 油门居中：定高模式（位置环+速度环）
				float altError = altitudeTarget - altFiltered;
				float targetVelZ = altPosPID.update(altError);
				float velError = targetVelZ - verticalVelocity;
				float altCorrection = altVelPID.update(velError);
				altCorrection = constrain(altCorrection, -ALT_CORRECTION_MAX, ALT_CORRECTION_MAX);
				thrustTarget = controlThrottle + altCorrection;
			} else {
				// 油门偏离：垂直速度模式
				// 摇杆输入 → 目标垂直速度（上推=上升，下推=下降）
				// throttleCmd 已是 -1~1（经 applyDeadband 归一化），直接乘最大垂直速度
				float targetVelZ = throttleCmd * HOVER_MAX_VERT_VEL;
				float velError = targetVelZ - verticalVelocity;
				float altCorrection = altVelPID.update(velError);
				altCorrection = constrain(altCorrection, -ALT_CORRECTION_MAX, ALT_CORRECTION_MAX);
				thrustTarget = controlThrottle + altCorrection;
				// 实时更新目标高度，松杆时无缝衔接
				altitudeTarget = altFiltered;
				altPosPID.reset();
			}
			thrustTarget = constrain(thrustTarget, 0.0f, 1.0f);
		}

		// ----- 3. 偏航控制（大疆风格：摇杆=偏航角速率）-----
		float yawTarget = attitudeTarget.getYaw();
		if (!armed || invalid(yawTarget)) yawTarget = attitude.getYaw();
		if (controlYaw != 0) {
			// 偏航摇杆有输入：控制偏航角速率，yawTarget 跟随当前航向
			yawTarget = attitude.getYaw();
			ratesTarget.z = -controlYaw * maxRate.z;
		} else {
			// 偏航摇杆居中：锁定当前航向（yawPID 维持 yawTarget）
			ratesTarget.z = 0;
		}

		// 组装目标姿态：水平倾角由速度环决定，yaw 由偏航摇杆决定
		attitudeTarget = Quaternion::fromEuler(Vector(tiltCmd.x, tiltCmd.y, yawTarget));

	} else {
		// ============================================================
		// STAB 模式：原有逻辑不变
		// ============================================================
		float yawTarget = attitudeTarget.getYaw();
		if (!armed || invalid(yawTarget) || controlYaw != 0) yawTarget = attitude.getYaw();
		attitudeTarget = Quaternion::fromEuler(Vector(controlRoll * tiltMax, controlPitch * tiltMax, yawTarget));
		ratesTarget.z = -controlYaw * maxRate.z;
	}
}

/**
 * @brief 姿态角度控制(外环)
 *
 * 根据目标姿态和当前姿态的偏差，计算目标角速度。
 * 悬停模式下使用增强的 I 项，更好维持速度环输出的微倾姿态。
 */
void controlAttitude() {
	if (!armed || attitudeTarget.invalid()) return;

	// 低油门积分保护（参考 dRonin lowThrottleZeroIntegral）
	// 油门低于阈值时清零所有 PID 积分项，防止地面/怠速时积分累积导致起飞跳变。
	// 注意：此处覆盖原 thrustTarget < 0.1 的早退逻辑，阈值提升到 0.12，
	//       在低油门区间（含原 0.1~0.12）持续清零积分，确保起飞瞬间无积分残留。
	if (thrustTarget < LOW_THROTTLE_INTEGRAL_CLEAR) {
		rollPID.reset();
		pitchPID.reset();
		yawPID.reset();
		rollRatePID.reset();
		pitchRatePID.reset();
		yawRatePID.reset();
		altPosPID.reset();
		altVelPID.reset();
		hoverCtrl.reset();
		return;  // 低油门时不做姿态控制
	}

	const Vector up(0, 0, 1);
	Vector upActual = Quaternion::rotateVector(up, attitude);
	Vector upTarget = Quaternion::rotateVector(up, attitudeTarget);

	// 姿态误差计算：使用 rotationVectorBetween 得到“从 upTarget 到 upActual”的旋转向量
	// 该方法已验证工作正常，符号与下方 PID 一致，不做改动。
	//
	// dRonin 替代方法（参考，未启用）：
	//   dRonin 用 atti^-1 * desired 计算误差，符号约定与本实现相反。
	//   若要改用四元数乘法，需保持与原符号一致，应写为：
	//     Quaternion errorQuat = attitude * attitudeTarget.inversed();
	//     Vector error = errorQuat.toRotationVector();
	//   （attitude * attitudeTarget.inversed() 表示“从目标到当前”的旋转，
	//    与原 rotationVectorBetween(upTarget, upActual) 方向一致。）
	//   注意：toRotationVector() 返回轴*角向量，与原返回类型一致。
	//   因四元数方法符号有歧义风险，且原方法已稳定，此处保持不变。
	Vector error = Vector::rotationVectorBetween(upTarget, upActual);

	// HOVER 模式：临时增强 I 项以更好维持速度环输出的目标倾角
	float savedRollI = rollPID.i;
	float savedPitchI = pitchPID.i;
	if (mode == OBSTACLE) {
		rollPID.i = 0.01f;
		pitchPID.i = 0.01f;
	}

	ratesTarget.x = rollPID.update(error.x);
	ratesTarget.y = pitchPID.update(error.y);

	if (mode == OBSTACLE) {
		rollPID.i = savedRollI;
		pitchPID.i = savedPitchI;
	}

	float yawError = wrapAngle(radians(attitudeTarget.getYaw() - attitude.getYaw()));
	ratesTarget.z = yawPID.update(yawError);
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
		case STAB: return "STAB";
		case OBSTACLE: return "HOVER";
		default: return "UNKNOWN";
	}
}
