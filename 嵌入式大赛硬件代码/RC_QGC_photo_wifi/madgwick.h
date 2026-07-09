/**
 * @file madgwick.h
 * @brief Madgwick AHRS 姿态估计算法
 * @author Sebastian Madgwick
 * @date 2020
 * @version 1.0
 * 
 * 基于梯度下降的四元数姿态估计算法。
 * 融合陀螺仪和加速度计数据，提供比互补滤波更高的精度。
 * 通过编译宏 USE_MADGWICK_FILTER 启用。
 * 
 * 原始论文: "An efficient orientation filter for inertial and
 * inertial/magnetic sensor arrays"
 */

#pragma once

#include "vector.h"
#include "quaternion.h"

/** Madgwick 滤波器增益 (默认0.1) */
float madgwickBeta = 0.1f;

/**
 * @brief Madgwick AHRS 姿态更新
 * 
 * @param q 当前姿态四元数（输入/输出）
 * @param gx 陀螺仪X轴角速度 (rad/s)
 * @param gy 陀螺仪Y轴角速度 (rad/s)
 * @param gz 陀螺仪Z轴角速度 (rad/s)
 * @param ax 加速度计X轴 (m/s²)
 * @param ay 加速度计Y轴 (m/s²)
 * @param az 加速度计Z轴 (m/s²)
 * @param dt 时间步长 (s)
 */
void madgwickUpdate(Quaternion& q, float gx, float gy, float gz, float ax, float ay, float az, float dt) {
    float q0 = q.w, q1 = q.x, q2 = q.y, q3 = q.z;
    float recipNorm;
    float s0, s1, s2, s3;
    float qDot1, qDot2, qDot3, qDot4;

    // Rate of change of quaternion from gyroscope
    qDot1 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
    qDot2 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
    qDot3 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
    qDot4 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);

    // Compute feedback only if accelerometer measurement valid
    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
        // Normalise accelerometer measurement
        recipNorm = 1.0f / sqrtf(ax * ax + ay * ay + az * az);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Gradient descent algorithm corrective step
        s0 = q2 * 2.0f * (2.0f * q0 * q0 - 1.0f + 2.0f * q2 * q2) + q1 * 2.0f * (2.0f * q0 * q1 + 2.0f * q2 * q3 - az) - q3 * 2.0f * (2.0f * q0 * q3 - 2.0f * q1 * q2 - ay);
        s1 = q3 * 2.0f * (2.0f * q0 * q0 - 1.0f + 2.0f * q3 * q3) + q2 * 2.0f * (2.0f * q0 * q2 - 2.0f * q1 * q3 - ax) + q0 * 2.0f * (2.0f * q0 * q1 + 2.0f * q2 * q3 - az);
        s2 = q1 * 2.0f * (2.0f * q0 * q0 - 1.0f + 2.0f * q1 * q1) - q3 * 2.0f * (2.0f * q0 * q3 - 2.0f * q1 * q2 - ay) + q0 * 2.0f * (2.0f * q0 * q2 - 2.0f * q1 * q3 - ax);
        s3 = q2 * 2.0f * (2.0f * q0 * q0 - 1.0f + 2.0f * q2 * q2) - q1 * 2.0f * (2.0f * q0 * q1 + 2.0f * q2 * q3 - az) + q0 * 2.0f * (2.0f * q0 * q3 - 2.0f * q1 * q2 - ay);
        recipNorm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
        s0 *= recipNorm;
        s1 *= recipNorm;
        s2 *= recipNorm;
        s3 *= recipNorm;

        // Apply feedback step
        qDot1 -= madgwickBeta * s0;
        qDot2 -= madgwickBeta * s1;
        qDot3 -= madgwickBeta * s2;
        qDot4 -= madgwickBeta * s3;
    }

    // Integrate rate of change of quaternion to yield quaternion
    q.w += qDot1 * dt;
    q.x += qDot2 * dt;
    q.y += qDot3 * dt;
    q.z += qDot4 * dt;

    // Normalise quaternion
    recipNorm = 1.0f / sqrtf(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
    q.w *= recipNorm;
    q.x *= recipNorm;
    q.y *= recipNorm;
    q.z *= recipNorm;
}