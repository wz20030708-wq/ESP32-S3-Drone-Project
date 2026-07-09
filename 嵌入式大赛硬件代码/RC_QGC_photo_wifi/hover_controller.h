/**
 * @file hover_controller.h
 * @brief 
 * @date 2026
 * @version 3.0
 *
 * 大疆风格操作逻辑：
 *   - 方向摇杆 = 水平速度指令（居中=0=悬停，偏离=按速度移动）
 *   - 油门杆 = 垂直速度指令（居中=定高，偏离=升降）
 *   - 偏航摇杆 = 偏航角速率
 *   - 松杆 = 自动悬停
 *
 * 控制架构（4层级联，由外到内）：
 *   位置环 → 速度环 → 姿态环 → 角速度环
 *
 * 速度环工作原理：
 *   - 目标速度来自摇杆输入（世界坐标系）
 *   - 实际速度由加速度计积分估计（含指数衰减防漂移）
 *   - 速度误差 → PID → 目标倾角
 *   - 摇杆居中时目标速度=0，自动悬停
 */

#pragma once

#include "pid.h"
#include "lpf.h"
#include "vector.h"
#include "quaternion.h"
#include "util.h"  // 提供 ONE_G 重力常量，用于加速度去重力

// ============== 悬停控制器增益参数 ==============
// 加速度直接反馈增益 (rad·s²/m)
// 检测到水平加速度时立即反向倾斜产生反向推力，实现"检测到移动→反向修正"的快速响应。
// 注意：增益过大会放大加速度计噪声/振动，导致起飞翻倒！
// 0.05 表示 1 m/s² 加速度产生 0.05 rad ≈ 2.9° 反向倾角。
#define HOVER_ACC_FEEDBACK_GAIN 0.05f
// 加速度反馈死区 (m/s²)
// 低于此值的加速度不反馈，过滤起飞瞬间电机冲击和振动噪声。
#define HOVER_ACC_DEADBAND  0.5f
// 加速度反馈输出限幅 (rad)，防止单次冲击产生过大倾角
#define HOVER_ACC_FB_LIM    0.05f   // 约 3°
// 速度死区阈值 (m/s)
// 仅在目标速度为 0（悬停）时生效，防止加速度噪声积分导致速度漂移。
#define HOVER_VEL_DEADBAND 0.02f

/**
 * @class HoverController
 * @brief 悬停模式级联控制器
 *
 * 封装水平速度估计 + 速度PID + 倾角限幅。
 * 输入：加速度（机体系）、当前姿态、dt、目标水平速度（世界系）
 * 输出：目标 roll/pitch 倾角（机体系，rad）
 */
class HoverController {
public:
	/**
	 * @brief 配置参数
	 * @param velKp 速度环P
	 * @param velKi 速度环I (消除稳态漂移)
	 * @param velKd 速度环D (抑制振荡)
	 * @param velWindup 速度环积分饱和
	 * @param tiltMax 最大倾角限幅 (rad)
	 * @param velEstTau 速度估计衰减时间常数 (s)
	 * @param maxHorizVel 最大水平速度目标 (m/s)
	 */
	void begin(float velKp, float velKi, float velKd,
			   float velWindup, float tiltMax, float velEstTau, float maxHorizVel) {
		// PID 构造第 5 参数 dAlpha=0.1f：D 项已内置低通滤波，抑制高频噪声
		_velXPID = PID(velKp, velKi, velKd, velWindup, 0.1f);
		_velYPID = PID(velKp, velKi, velKd, velWindup, 0.1f);
		_tiltMax = tiltMax;
		_velEstTau = velEstTau;
		_maxHorizVel = maxHorizVel;
	}

	/**
	 * @brief 重置控制器状态
	 *
	 * 进入悬停模式、油门杆操控中、或检测到异常时调用。
	 * 清空速度估计和 PID 积分项。
	 *
	 * 注意：不清零 _accBiasX/Y，保留已学习的加速度零偏估计结果，
	 * 避免模式切换导致零偏丢失、需要重新收敛。
	 */
	void reset() {
		_velX = 0; _velY = 0;
		_velXPID.reset();
		_velYPID.reset();
		_accFilterX.reset();
		_accFilterY.reset();
		// _lastOutput 也清零，避免复位后立即输出陈旧指令
		_lastOutput = Vector(0, 0, 0);
	}

	/**
	 * @brief 更新悬停控制
	 * @param accBody 机体系加速度 (m/s²)
	 * @param attitude 当前姿态四元数
	 * @param dt 时间步长 (s)
	 * @param velTargetX 世界系 X 方向目标速度 (m/s)，0=悬停
	 * @param velTargetY 世界系 Y 方向目标速度 (m/s)，0=悬停
	 * @param landed 是否已着陆（true 时更新加速度零偏估计）
	 * @return 目标倾角 Vector(roll, pitch, 0) (rad, 机体系)
	 *
	 * 流程：
	 *   1. 加速度 → 世界坐标系 → 去除重力 → 提取水平分量 → 低通滤波
	 *   2. 着陆时 EMA 更新加速度零偏，并从加速度中扣除零偏
	 *   3. 积分得速度估计，含指数衰减防漂移 + 死区（悬停时防噪）
	 *   4. 速度误差 (velTarget - velEst) → PID → 目标倾角（稳态修正）
	 *   4.5 加速度直接反馈 → 倾角（快速响应，不依赖积分）
	 *   5. 倾角限幅
	 *
	 * 当 velTarget=0 时（摇杆居中），等价于"悬停锁定"行为
	 * 当 velTarget≠0 时（摇杆偏离），无人机以目标速度移动
	 */
	Vector update(const Vector& accBody, const Quaternion& attitude, float dt,
				  float velTargetX = 0, float velTargetY = 0, bool landed = false) {
		// dt 异常处理：dt<=0 时保留上次输出，dt 过大时 clamp 而非返回零
		// 避免瞬时丢帧导致输出突变为零，造成姿态抖动
		if (dt <= 0) return _lastOutput;
		if (dt > 0.1f) dt = 0.1f;

		// 目标速度限幅
		velTargetX = constrain(velTargetX, -_maxHorizVel, _maxHorizVel);
		velTargetY = constrain(velTargetY, -_maxHorizVel, _maxHorizVel);

		// ===== 1. 加速度转世界坐标系，去除重力，提取水平分量 =====
		Vector accWorld = Quaternion::rotateVector(accBody, attitude);
		// 核心修复：加速度计测量值包含重力，机身倾斜时旋转到世界系后，
		// 重力会污染水平分量 X/Y，导致速度估计严重失真。从 Z 分量扣除重力后，
		// X/Y 才是真实的水平运动加速度。
		accWorld.z -= ONE_G;
		float accX = _accFilterX.update(accWorld.x);
		float accY = _accFilterY.update(accWorld.y);

		// ===== 2. 加速度零偏估计与扣除 =====
		// 着陆时（无运动、加速度计理论上应读零）用 EMA 缓慢学习零偏
		// 系数 0.005：约 200 个采样收敛，避免飞行中误学
		if (landed) {
			_accBiasX = _accBiasX * 0.995f + accX * 0.005f;
			_accBiasY = _accBiasY * 0.995f + accY * 0.005f;
		}
		// 扣除零偏，得到真实运动加速度
		accX -= _accBiasX;
		accY -= _accBiasY;

		// ===== 3. 速度估计（complementary + 指数衰减）=====
		_velX += accX * dt;
		_velY += accY * dt;
		// 指数衰减：防止积分饱和和长期漂移
		float decay = expf(-dt / _velEstTau);
		_velX *= decay;
		_velY *= decay;
		// 速度限幅
		const float VEL_LIM = 2.0f;
		_velX = constrain(_velX, -VEL_LIM, VEL_LIM);
		_velY = constrain(_velY, -VEL_LIM, VEL_LIM);
		// 速度死区：很小的速度强制归零，防止加速度噪声积分导致速度漂移
		// 仅在目标速度为 0（悬停）时应用死区，移动时不应用，避免影响追踪精度
		if (fabsf(_velX) < HOVER_VEL_DEADBAND && velTargetX == 0) _velX = 0;
		if (fabsf(_velY) < HOVER_VEL_DEADBAND && velTargetY == 0) _velY = 0;

		// ===== 4. 速度 PID（目标速度 - 估计速度）=====
		// 大疆逻辑：摇杆居中时 velTarget=0，PID 自动悬停
		//          摇杆偏离时 velTarget≠0，无人机追踪目标速度
		float velErrorX = velTargetX - _velX;
		float velErrorY = velTargetY - _velY;
		float tiltCmdX = _velXPID.update(velErrorX);
		float tiltCmdY = _velYPID.update(velErrorY);

		// ===== 4.5 加速度直接反馈（快速响应，不依赖积分）=====
		// 检测到水平加速度 → 立即反向倾斜产生反向推力
		// 这实现了"检测到移动→反向修正"的快速响应
		// accX 为正表示向 X 正向加速，需要负倾角（反向倾斜）来抵消
		// 注意：accX/accY 已经是滤波后、去零偏的水平加速度（世界系），单位 m/s²
		// 安全保护：死区过滤噪声 + 限幅防冲击 + 增益保守，避免起飞翻倒
		float accFbX = 0, accFbY = 0;
		if (fabsf(accX) > HOVER_ACC_DEADBAND) {
			accFbX = -accX * HOVER_ACC_FEEDBACK_GAIN;
			accFbX = constrain(accFbX, -HOVER_ACC_FB_LIM, HOVER_ACC_FB_LIM);
		}
		if (fabsf(accY) > HOVER_ACC_DEADBAND) {
			accFbY = -accY * HOVER_ACC_FEEDBACK_GAIN;
			accFbY = constrain(accFbY, -HOVER_ACC_FB_LIM, HOVER_ACC_FB_LIM);
		}
		tiltCmdX += accFbX;
		tiltCmdY += accFbY;

		// ===== 5. 倾角限幅 =====
		tiltCmdX = constrain(tiltCmdX, -_tiltMax, _tiltMax);
		tiltCmdY = constrain(tiltCmdY, -_tiltMax, _tiltMax);

		// 世界系倾角指令 → 机体系倾角
		// 需要将世界系的 (X方向倾角, Y方向倾角) 转换为机体系的 (roll, pitch)
		// 用当前姿态的逆向旋转（世界系→机体系）
		Vector tiltWorld(tiltCmdX, tiltCmdY, 0);
		Vector tiltBody = attitude.conjugate(tiltWorld);  // 世界系→机体系
		// tiltBody.x ≈ roll, tiltBody.y ≈ pitch (小角度近似)
		// 缓存本次输出，用于 dt 异常时返回上次有效值
		return _lastOutput = Vector(tiltBody.x, tiltBody.y, 0);
	}

	/** 获取当前水平速度估计 (m/s, 世界坐标系) */
	Vector getVelocityEstimate() const {
		return Vector(_velX, _velY, 0);
	}

	/** 获取速度环 PID 状态（调试用） */
	void getPIDState(float& velXErr, float& velYErr, float& velXInt, float& velYInt) const {
		velXErr = -_velX;  // 悬停时目标=0，误差=-vel
		velYErr = -_velY;
		velXInt = _velXPID.integral;
		velYInt = _velYPID.integral;
	}

private:
	// 速度环 PID（X/Y 方向各一个）
	PID _velXPID = PID(0, 0, 0);
	PID _velYPID = PID(0, 0, 0);
	// 加速度低通滤波器（α=0.08，更平滑，抑制高频噪声与振动耦合）
	LowPassFilter<float> _accFilterX = LowPassFilter<float>(0.08f);
	LowPassFilter<float> _accFilterY = LowPassFilter<float>(0.08f);
	// 速度估计
	float _velX = 0, _velY = 0;
	// 加速度零偏估计（着陆时学习，飞行时使用）
	// 不在 reset() 中清零，保留跨模式学习结果
	float _accBiasX = 0, _accBiasY = 0;
	// 上次有效输出（dt 异常时返回，避免输出突变）
	Vector _lastOutput = Vector(0, 0, 0);
	// 配置
	float _tiltMax = radians(12);
	float _velEstTau = 1.0f;  // 衰减时间常数：1.0s，平衡漂移抑制与响应保持
	float _maxHorizVel = 1.0f;
};
