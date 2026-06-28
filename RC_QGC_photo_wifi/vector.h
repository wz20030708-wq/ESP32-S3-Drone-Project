/**
 * @file vector.h
 * @brief 三维向量类实现
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 提供飞控所需的三维向量运算，支持基本算术运算、点积、叉积、归一化等操作。
 * 继承Printable接口，支持Serial.print直接输出。
 */

#pragma once

#include <Printable.h>

/**
 * @class Vector
 * @brief 三维浮点向量类
 * 
 * 封装三轴浮点数据，提供飞控所需的向量运算功能。
 * 继承Printable接口，支持Serial.print直接输出。
 */
class Vector : public Printable {
public:
	/** X轴分量 */
	float x, y, z;

	/**
	 * @brief 无参构造函数，初始化三轴数据为0
	 */
	Vector(): x(0), y(0), z(0) {};

	/**
	 * @brief 带参构造函数，直接传入三轴数据
	 * @param x X轴分量
	 * @param y Y轴分量
	 * @param z Z轴分量
	 */
	Vector(float x, float y, float z): x(x), y(y), z(z) {};

	/**
	 * @brief 判断三轴数据是否全部为0
	 * @return true表示零向量，false表示非零向量
	 */
	bool zero() const {
		return x == 0 && y == 0 && z == 0;
	}

	/**
	 * @brief 判断三轴数据是否为有效有限浮点数
	 * @return true表示有效，false表示包含NAN或INF
	 */
	bool finite() const {
		return isfinite(x) && isfinite(y) && isfinite(z);
	}

	/**
	 * @brief 判断数据是否有效(语义封装)
	 * @return true表示有效，false表示无效
	 */
	bool valid() const {
		return finite();
	}

	/**
	 * @brief 判断数据是否无效(反向判断)
	 * @return true表示无效，false表示有效
	 */
	bool invalid() const {
		return !valid();
	}

	/**
	 * @brief 将向量标记为无效(NAN)
	 */
	void invalidate() {
		x = NAN;
		y = NAN;
		z = NAN;
	}

	/**
	 * @brief 计算向量模长(欧几里得范数)
	 * @return 向量模长
	 */
	float norm() const {
		return sqrt(x * x + y * y + z * z);
	}

	/**
	 * @brief 向量归一化(单位化)
	 */
	void normalize() {
		float n = norm();
		x /= n;
		y /= n;
		z /= n;
	}

	/**
	 * @brief 向量 + 标量(逐轴相加)
	 */
	Vector operator + (const float b) const {
		return Vector(x + b, y + b, z + b);
	}

	/**
	 * @brief 向量 * 标量(逐轴相乘)
	 */
	Vector operator * (const float b) const {
		return Vector(x * b, y * b, z * b);
	}

	/**
	 * @brief 向量 / 标量(逐轴相除)
	 */
	Vector operator / (const float b) const {
		return Vector(x / b, y / b, z / b);
	}

	/**
	 * @brief 向量 + 向量(逐轴相加)
	 */
	Vector operator + (const Vector& b) const {
		return Vector(x + b.x, y + b.y, z + b.z);
	}

	/**
	 * @brief 向量 - 向量(逐轴相减)
	 */
	Vector operator - (const Vector& b) const {
		return Vector(x - b.x, y - b.y, z - b.z);
	}

	/**
	 * @brief 向量 += 向量(原地修改)
	 */
	Vector& operator += (const Vector& b) {
		return *this = *this + b;
	}

	/**
	 * @brief 向量 -= 向量(原地修改)
	 */
	Vector& operator -= (const Vector& b) {
		return *this = *this - b;
	}

	/**
	 * @brief 向量 * 向量(逐元素相乘)
	 */
	Vector operator * (const Vector& b) const {
		return Vector(x * b.x, y * b.y, z * b.z);
	}

	/**
	 * @brief 向量 / 向量(逐元素相除)
	 */
	Vector operator / (const Vector& b) const {
		return Vector(x / b.x, y / b.y, z / b.z);
	}

	/**
	 * @brief 判断两个向量是否相等(逐轴比较)
	 */
	bool operator == (const Vector& b) const {
		return x == b.x && y == b.y && z == b.z;
	}

	/**
	 * @brief 判断两个向量是否不相等
	 */
	bool operator != (const Vector& b) const {
		return !(*this == b);
	}

	/**
	 * @brief 计算两个向量的点积(内积)
	 * @param a 向量a
	 * @param b 向量b
	 * @return 点积结果
	 */
	static float dot(const Vector& a, const Vector& b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	/**
	 * @brief 计算两个向量的叉积(外积)
	 * @param a 向量a
	 * @param b 向量b
	 * @return 叉积结果向量
	 */
	static Vector cross(const Vector& a, const Vector& b) {
		return Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
	}

	/**
	 * @brief 计算两个向量的夹角(弧度)
	 * @param a 向量a
	 * @param b 向量b
	 * @return 夹角(弧度)
	 */
	static float angleBetween(const Vector& a, const Vector& b) {
		return acos(constrain(dot(a, b) / (a.norm() * b.norm()), -1, 1));
	}

	/**
	 * @brief 计算从向量a到向量b的旋转向量
	 * @param a 起始向量
	 * @param b 目标向量
	 * @return 旋转向量(轴角表示)
	 */
	static Vector rotationVectorBetween(const Vector& a, const Vector& b) {
		Vector direction = cross(a, b);
		if (direction.zero()) {
			if (dot(a, b) > 0) {
				return Vector(0, 0, 0);
			}
			Vector axis = (fabs(a.x) < 0.99f * a.norm()) ? cross(a, Vector(1, 0, 0))
			                                                : cross(a, Vector(0, 1, 0));
			axis.normalize();
			return axis * PI * a.norm();
		}
		direction.normalize();
		float angle = angleBetween(a, b);
		return direction * angle;
	}

	/**
	 * @brief 实现Printable接口，支持Serial.print输出
	 * @param p Print对象
	 * @return 输出字符数
	 */
	size_t printTo(Print& p) const {
		return
			p.print(x, 15) + p.print(" ") +
			p.print(y, 15) + p.print(" ") +
			p.print(z, 15);
	}
};

/**
 * @brief 标量 * 向量(全局重载，支持标量在前)
 */
Vector operator * (const float a, const Vector& b) { return b * a; }

/**
 * @brief 标量 + 向量(全局重载，支持标量在前)
 */
Vector operator + (const float a, const Vector& b) { return b + a; }