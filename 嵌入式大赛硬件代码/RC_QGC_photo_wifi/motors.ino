/**
 * @file motors.ino
 * @brief 电机驱动控制模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 使用ESP32 LEDC PWM输出控制四旋翼无人机的四个电机。
 * 支持两种驱动配置：MOSFET直流电机直驱或ESC无刷电机驱动。
 */

#include "util.h"

/** 电机0引脚(后左) */
#define MOTOR_0_PIN 41
/** 电机1引脚(后右) */
#define MOTOR_1_PIN 40
/** 电机2引脚(前右) */
#define MOTOR_2_PIN 39
/** 电机3引脚(前左) */
#define MOTOR_3_PIN 42

// ==================== 电机驱动配置 (二选一) ====================

// [配置1] ESC电调 / 无刷电机 
//#define PWM_FREQUENCY 400
//#define PWM_RESOLUTION 16
//#define PWM_STOP    0
//#define PWM_MIN     1000
//#define PWM_MAX     2000

// [配置2] MOSFET / 直流电机
#define PWM_FREQUENCY 20000
#define PWM_RESOLUTION 10
#define PWM_STOP    0
#define PWM_MIN     0
#define PWM_MAX     1000000 / PWM_FREQUENCY
// ================================================================

/** 电机索引定义 */
const int MOTOR_REAR_LEFT = 0;
const int MOTOR_REAR_RIGHT = 1;
const int MOTOR_FRONT_RIGHT = 2;
const int MOTOR_FRONT_LEFT = 3;

/**
 * @brief 初始化电机驱动
 */
void setupMotors() {
	print("Setup Motors\n");

	pinMode(MOTOR_0_PIN, OUTPUT); digitalWrite(MOTOR_0_PIN, LOW);
	pinMode(MOTOR_1_PIN, OUTPUT); digitalWrite(MOTOR_1_PIN, LOW);
	pinMode(MOTOR_2_PIN, OUTPUT); digitalWrite(MOTOR_2_PIN, LOW);
	pinMode(MOTOR_3_PIN, OUTPUT); digitalWrite(MOTOR_3_PIN, LOW);

	ledcAttach(MOTOR_0_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
	ledcAttach(MOTOR_1_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
	ledcAttach(MOTOR_2_PIN, PWM_FREQUENCY, PWM_RESOLUTION);
	ledcAttach(MOTOR_3_PIN, PWM_FREQUENCY, PWM_RESOLUTION);

	sendMotors();
	print("Motors initialized\n");
}

/**
 * @brief 将电机输出值转换为PWM占空比
 * @param value 电机输出值(0~1)
 * @return PWM占空比值
 */
int getDutyCycle(float value) {
	value = constrain(value, 0, 1);
	float pwm = mapf(value, 0, 1, PWM_MIN, PWM_MAX);
	if (value == 0) pwm = PWM_STOP;
	float periodUs = 1000000.0f / PWM_FREQUENCY;
	float duty = mapf(pwm, 0, periodUs, 0, (1 << PWM_RESOLUTION) - 1);
	return round(duty);
}

/**
 * @brief 发送电机输出到PWM引脚
 */
void sendMotors() {
	ledcWrite(MOTOR_0_PIN, getDutyCycle(motors[0]));
	ledcWrite(MOTOR_1_PIN, getDutyCycle(motors[1]));
	ledcWrite(MOTOR_2_PIN, getDutyCycle(motors[2]));
	ledcWrite(MOTOR_3_PIN, getDutyCycle(motors[3]));
}

/**
 * @brief 判断电机是否活跃
 * @return true表示至少有一个电机输出不为0
 */
bool motorsActive() {
	return motors[0] != 0 || motors[1] != 0 || motors[2] != 0 || motors[3] != 0;
}

/**
 * @brief 测试单个电机
 * @param n 电机编号(0~3)
 */
void testMotor(int n) {
	print("Testing motor %d\n", n);
	motors[n] = 1;
	delay(50); 
	sendMotors();
	pause(3);
	motors[n] = 0;
	sendMotors();
	print("Done\n");
}