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

/** 加速度计修正权重 */
float accWeight = 0.003;
/** 角速度低通滤波器 */
LowPassFilter<Vector> ratesFilter(0.2);

/**
 * @brief 姿态估计主入口
 * 
 * 依次执行陀螺仪积分和加速度计修正。
 */
void estimate() {
	applyGyro();
	applyAcc();
}

/**
 * @brief 应用陀螺仪数据更新姿态
 * 
 * 通过对角速度积分，更新姿态四元数。
 * 角速度先经过低通滤波去除高频噪声。
 */
void applyGyro() {
	rates = ratesFilter.update(gyro);
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(rates * dt));
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
	landed = !motorsActive() && abs(accNorm - ONE_G) < ONE_G * 0.1f;

	bool useFullWeight = landed || thrustTarget < 0.12;

	float correctionWeight = 0;
	if (useFullWeight) {
		correctionWeight = accWeight;
	} else if (abs(accNorm - ONE_G) < ONE_G * 0.3f) {
		correctionWeight = accWeight * 0.1f;
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