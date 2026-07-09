/**
 * @file estimate.ino
 * @brief 姿态估计模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 基于扩展卡尔曼滤波思想，融合陀螺仪和加速度计数据进行姿态估计。
 * 陀螺仪积分预测姿态，加速度计提供重力向量参考进行修正。
 */

#include "quaternion.h"
#include "vector.h"
#include "lpf.h"
#include "util.h"
#include "madgwick.h"

/** 加速度计修正权重 */
float accWeight = 0.003;
/** 角速度低通滤波器 */
LowPassFilter<Vector> ratesFilter(0.2);

/** 垂直速度估计值 (m/s, 向上为正) */
float verticalVelocity = 0;
/** 气压计高度低通滤波器 */
LowPassFilter<float> altFilterLPF(0.1);
/** 滤波后的气压计高度 */
float altFiltered = 0;
/** 垂直速度低通滤波器 */
LowPassFilter<float> velFilter(0.2);

/**
 * @brief 姿态估计主入口
 * 
 * 依次执行陀螺仪积分和加速度计修正。
 */
void estimate() {
#ifdef USE_MADGWICK_FILTER
	// Madgwick AHRS: single-step fusion of gyro and accel
	estimateGyroBias();
	Vector gyroCorrected = gyro - gyroBias;
	madgwickUpdate(attitude, gyroCorrected.x, gyroCorrected.y, gyroCorrected.z, acc.x, acc.y, acc.z, dt);
	rates = gyroCorrected;
#else
	// Original complementary filter approach
	estimateGyroBias();
	applyGyro();
	applyAcc();
#endif
	// 垂直速度估计：使用气压计和加速度计融合
	if (altitudeM > 0) altFiltered = altFilterLPF.update(altitudeM);
	estimateVerticalVelocity();
}

/**
 * @brief 应用陀螺仪数据更新姿态
 * 
 * 通过对角速度积分，更新姿态四元数。
 * 角速度先经过低通滤波去除高频噪声。
 */
void applyGyro() {
	Vector gyroCorrected = gyro - gyroBias;
	rates = ratesFilter.update(gyroCorrected);
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(rates * dt));
}

/**
 * @brief 运行时陀螺仪零偏估计
 * 在着陆状态下通过EMA缓慢更新零偏，补偿温漂。
 */
void estimateGyroBias() {
	static float landedTime = 0;
	if (landed && !armed) {
		landedTime += dt;
		if (landedTime > 2.0f) {
			updateGyroBias(gyro);
		}
	} else {
		landedTime = 0;
	}
}

/**
 * @brief 应用加速度计数据修正姿态
 * 
 * 根据加速度计测量的重力方向，修正陀螺仪积分带来的漂移。
 * 修正权重根据飞行状态动态调整：
 * - 着陆或怠速：全权重快速收敛
 * - 飞行中：中等权重修正陀螺仪漂移
 */
void applyAcc() {
	float accNorm = acc.norm();
	landed = !motorsActive() && fabsf(accNorm - ONE_G) < ONE_G * 0.1f;

	bool useFullWeight = landed || thrustTarget < 0.12;

	float correctionWeight = 0;
	if (useFullWeight) {
		correctionWeight = accWeight;
	} else if (fabsf(accNorm - ONE_G) < ONE_G * 0.3f) {
		correctionWeight = accWeight * 0.33f;  // was 0.1f, now ~0.001
	}
	if (correctionWeight == 0) return;

	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	Vector correction = Vector::rotationVectorBetween(acc, up) * correctionWeight;

	if (!useFullWeight) {
		float maxCorrection = radians(1.0);
		float n = correction.norm();
		if (n > maxCorrection) {
			correction = correction / n * maxCorrection;
		}
	}

	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(correction));
}

/**
 * @brief 垂直速度估计
 * 
 * 通过互补滤波融合加速度计积分和气压计高度差分，
 * 得到平滑的垂直速度估计值。
 * 仅在气压计数据有效（altFiltered > 0）时运行。
 */
void estimateVerticalVelocity() {
	static float velAcc = 0;
	static float lastAltFiltered = 0;

	if (altFiltered <= 0) return;

	// 将加速度转换到世界坐标系，提取垂直分量并移除重力
	Vector accWorld = Quaternion::rotateVector(acc, attitude);
	float accVertical = accWorld.z - ONE_G;

	// 加速度积分得到速度
	velAcc += accVertical * dt;

	// 气压计高度差分得到速度
	float baroVel = (altFiltered - lastAltFiltered) / dt;

	// 互补滤波融合：加速度积分主导，气压计修正漂移
	velAcc = velAcc * 0.98f + baroVel * 0.02f;

	// 低通滤波输出
	verticalVelocity = velFilter.update(velAcc);

	lastAltFiltered = altFiltered;
}
