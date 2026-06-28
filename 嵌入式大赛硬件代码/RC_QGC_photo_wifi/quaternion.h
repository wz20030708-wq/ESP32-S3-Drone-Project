/**
 * @file quaternion.h
 * @brief 四元数类实现
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 提供轻量级旋转四元数库，支持姿态表示、欧拉角转换、向量旋转等操作。
 * 四元数避免了欧拉角的万向锁问题，是飞控姿态表示的首选方式。
 */

#pragma once

#include "vector.h"

/**
 * @class Quaternion
 * @brief 四元数类
 * 
 * 表示三维空间中的旋转，避免欧拉角的万向锁问题。
 * 继承Printable接口，支持Serial.print直接输出。
 */
class Quaternion : public Printable {
public:
	/** 实部(w)和虚部(x,y,z) */
	float w, x, y, z;

	/**
	 * @brief 默认构造函数，初始化为单位四元数(无旋转)
	 */
	Quaternion(): w(1), x(0), y(0), z(0) {};

	/**
	 * @brief 带参构造函数
	 * @param w 实部
	 * @param x 虚部x
	 * @param y 虚部y
	 * @param z 虚部z
	 */
	Quaternion(float w, float x, float y, float z): w(w), x(x), y(y), z(z) {};

	/**
	 * @brief 从轴角表示创建四元数
	 * @param axis 旋转轴(单位向量)
	 * @param angle 旋转角度(弧度)
	 * @return 四元数
	 */
	static Quaternion fromAxisAngle(const Vector& axis, float angle) {
		float halfAngle = angle * 0.5;
		float sin2 = sin(halfAngle);
		float cos2 = cos(halfAngle);
		float sinNorm = sin2 / axis.norm();
		return Quaternion(cos2, axis.x * sinNorm, axis.y * sinNorm, axis.z * sinNorm);
	}

	/**
	 * @brief 从旋转向量创建四元数
	 * @param rotation 旋转向量(轴角表示，模长为角度)
	 * @return 四元数
	 */
	static Quaternion fromRotationVector(const Vector& rotation) {
		if (rotation.zero()) {
			return Quaternion();
		}
		return Quaternion::fromAxisAngle(rotation, rotation.norm());
	}

	/**
	 * @brief 从欧拉角创建四元数(Z-Y-X顺序)
	 * @param euler 欧拉角(弧度): x=roll, y=pitch, z=yaw
	 * @return 四元数
	 */
	static Quaternion fromEuler(const Vector& euler) {
		float cx = cos(euler.x / 2);
		float cy = cos(euler.y / 2);
		float cz = cos(euler.z / 2);
		float sx = sin(euler.x / 2);
		float sy = sin(euler.y / 2);
		float sz = sin(euler.z / 2);

		return Quaternion(
			cx * cy * cz + sx * sy * sz,
			sx * cy * cz - cx * sy * sz,
			cx * sy * cz + sx * cy * sz,
			cx * cy * sz - sx * sy * cz);
	}

	/**
	 * @brief 从两个向量创建旋转四元数(从u旋转到v)
	 * @param u 起始向量
	 * @param v 目标向量
	 * @return 四元数
	 */
	static Quaternion fromBetweenVectors(const Vector& u, const Vector& v) {
		float dot = u.x * v.x + u.y * v.y + u.z * v.z;
		float w1 = u.y * v.z - u.z * v.y;
		float w2 = u.z * v.x - u.x * v.z;
		float w3 = u.x * v.y - u.y * v.x;

		Quaternion ret(
			dot + sqrt(dot * dot + w1 * w1 + w2 * w2 + w3 * w3),
			w1,
			w2,
			w3);
		ret.normalize();
		return ret;
	}

	/**
	 * @brief 判断四元数是否为有限值
	 * @return true表示有效，false表示包含NAN或INF
	 */
	bool finite() const {
		return isfinite(w) && isfinite(x) && isfinite(y) && isfinite(z);
	}

	/**
	 * @brief 判断四元数是否有效
	 * @return true表示有效，false表示无效
	 */
	bool valid() const {
		return finite();
	}

	/**
	 * @brief 判断四元数是否无效
	 * @return true表示无效，false表示有效
	 */
	bool invalid() const {
		return !valid();
	}

	/**
	 * @brief 将四元数标记为无效(NAN)
	 */
	void invalidate() {
		w = NAN;
		x = NAN;
		y = NAN;
		z = NAN;
	}

	/**
	 * @brief 计算四元数模长
	 * @return 模长
	 */
	float norm() const {
		return sqrt(w * w + x * x + y * y + z * z);
	}

	/**
	 * @brief 四元数归一化
	 */
	void normalize() {
		float n = norm();
		w /= n;
		x /= n;
		y /= n;
		z /= n;
	}

	/**
	 * @brief 将四元数转换为轴角表示
	 * @param axis 输出旋转轴
	 * @param angle 输出旋转角度(弧度)
	 */
	void toAxisAngle(Vector& axis, float& angle) const {
		angle = acos(constrain(w, -1, 1)) * 2;
		axis.x = x / sin(angle / 2);
		axis.y = y / sin(angle / 2);
		axis.z = z / sin(angle / 2);
	}

	/**
	 * @brief 将四元数转换为旋转向量
	 * @return 旋转向量
	 */
	Vector toRotationVector() const {
		if (w == 1 && x == 0 && y == 0 && z == 0) return Vector(0, 0, 0);
		float angle;
		Vector axis;
		toAxisAngle(axis, angle);
		return angle * axis;
	}

	/**
	 * @brief 将四元数转换为欧拉角(Z-Y-X顺序)
	 * @return 欧拉角(弧度): x=roll, y=pitch, z=yaw
	 */
	Vector toEuler() const {
		Vector euler;
		float sqx = x * x;
		float sqy = y * y;
		float sqz = z * z;
		float sqw = w * w;
		float sarg = -2 * (x * z - w * y) / (sqx + sqy + sqz + sqw);
		if (sarg < -1) sarg = -1;
		if (sarg > 1) sarg = 1;
		if (sarg <= -0.99999) {
			euler.x = 0;
			euler.y = -0.5 * PI;
			euler.z = (x == 0 && y == 0) ? 0 : -2 * atan2(y, x);
		} else if (sarg >= 0.99999) {
			euler.x = 0;
			euler.y = 0.5 * PI;
			euler.z = (x == 0 && y == 0) ? 0 : 2 * atan2(y, x);
		} else {
			euler.x = atan2(2 * (y * z + w * x), sqw - sqx - sqy + sqz);
			euler.y = asin(sarg);
			euler.z = atan2(2 * (x * y + w * z), sqw + sqx - sqy - sqz);
		}
		return euler;
	}

	/**
	 * @brief 获取Roll角(弧度)
	 * @return Roll角
	 */
	float getRoll() const {
		return toEuler().x;
	}

	/**
	 * @brief 获取Pitch角(弧度)
	 * @return Pitch角
	 */
	float getPitch() const {
		return toEuler().y;
	}

	/**
	 * @brief 获取Yaw角(弧度)
	 * @return Yaw角
	 */
	float getYaw() const {
		return toEuler().z;
	}

	/**
	 * @brief 设置Roll角
	 * @param roll Roll角(弧度)
	 */
	void setRoll(float roll) {
		Vector euler = toEuler();
		*this = Quaternion::fromEuler(Vector(roll, euler.y, euler.z));
	}

	/**
	 * @brief 设置Pitch角
	 * @param pitch Pitch角(弧度)
	 */
	void setPitch(float pitch) {
		Vector euler = toEuler();
		*this = Quaternion::fromEuler(Vector(euler.x, pitch, euler.z));
	}

	/**
	 * @brief 设置Yaw角
	 * @param yaw Yaw角(弧度)
	 */
	void setYaw(float yaw) {
		Vector euler = toEuler();
		*this = Quaternion::fromEuler(Vector(euler.x, euler.y, yaw));
	}

	/**
	 * @brief 四元数乘法(旋转组合)
	 */
	Quaternion operator * (const Quaternion& q) const {
		return Quaternion(
			w * q.w - x * q.x - y * q.y - z * q.z,
			w * q.x + x * q.w + y * q.z - z * q.y,
			w * q.y + y * q.w + z * q.x - x * q.z,
			w * q.z + z * q.w + x * q.y - y * q.x);
	}

	/**
	 * @brief 判断四元数是否相等
	 */
	bool operator == (const Quaternion& q) const {
		return w == q.w && x == q.x && y == q.y && z == q.z;
	}

	/**
	 * @brief 判断四元数是否不相等
	 */
	bool operator != (const Quaternion& q) const {
		return !(*this == q);
	}

	/**
	 * @brief 计算四元数的逆
	 * @return 逆四元数
	 */
	Quaternion inversed() const {
		float normSqInv = 1 / (w * w + x * x + y * y + z * z);
		return Quaternion(
			w * normSqInv,
			-x * normSqInv,
			-y * normSqInv,
			-z * normSqInv);
	}

	/**
	 * @brief 用四元数旋转向量(前向旋转)
	 * @param v 待旋转向量
	 * @return 旋转后的向量
	 */
	Vector conjugate(const Vector& v) const {
		Quaternion qv(0, v.x, v.y, v.z);
		Quaternion res = (*this) * qv * inversed();
		return Vector(res.x, res.y, res.z);
	}

	/**
	 * @brief 用四元数旋转向量(逆向旋转)
	 * @param v 待旋转向量
	 * @return 旋转后的向量
	 */
	Vector conjugateInversed(const Vector& v) const {
		Quaternion qv(0, v.x, v.y, v.z);
		Quaternion res = inversed() * qv * (*this);
		return Vector(res.x, res.y, res.z);
	}

	/**
	 * @brief 用四元数b旋转四元数a
	 * @param a 原始四元数
	 * @param b 旋转四元数
	 * @param normalize 是否归一化(默认true)
	 * @return 旋转后的四元数
	 */
	static Quaternion rotate(const Quaternion& a, const Quaternion& b, const bool normalize = true) {
		Quaternion rotated = a * b;
		if (normalize) {
			rotated.normalize();
		}
		return rotated;
	}

	/**
	 * @brief 用四元数旋转向量
	 * @param v 待旋转向量
	 * @param q 旋转四元数
	 * @return 旋转后的向量
	 */
	static Vector rotateVector(const Vector& v, const Quaternion& q) {
		return q.conjugateInversed(v);
	}

	/**
	 * @brief 计算从四元数a到四元数b的旋转
	 * @param a 起始四元数
	 * @param b 目标四元数
	 * @param normalize 是否归一化(默认true)
	 * @return 旋转四元数
	 */
	static Quaternion between(const Quaternion& a, const Quaternion& b, const bool normalize = true) {
		Quaternion q = a * b.inversed();
		if (normalize) {
			q.normalize();
		}
		return q;
	}

	/**
	 * @brief 实现Printable接口，支持Serial.print输出
	 * @param p Print对象
	 * @return 输出字符数
	 */
	size_t printTo(Print& p) const {
		size_t r = 0;
		r += p.print(w, 15) + p.print(" ");
		r += p.print(x, 15) + p.print(" ");
		r += p.print(y, 15) + p.print(" ");
		r += p.print(z, 15);
		return r;
	}
};