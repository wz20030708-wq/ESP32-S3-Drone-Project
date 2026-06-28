/**
 * @file imu.ino
 * @brief IMU传感器驱动模块(MPU9250)
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 通过SPI总线与MPU9250惯性测量单元通信，获取陀螺仪和加速度计数据。
 * 支持陀螺仪动态校准和加速度计六面校准。
 */

#include <SPI.h>
#include <FlixPeriph.h>
#include "vector.h"
#include "lpf.h"
#include "util.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/** 飞行控制任务句柄 */
extern TaskHandle_t flightControlTaskHandle;

// ====================== IMU SPI 引脚宏定义 =======================

/** SPI时钟引脚 */
#define IMU_SCK_PIN     38
/** SPI数据输入引脚 */
#define IMU_MISO_PIN    36
/** SPI数据输出引脚 */
#define IMU_MOSI_PIN    37
/** SPI片选引脚 */
#define IMU_CS_PIN      35

// ============================================================================

SPIClass SPI_IMU;
MPU9250 imu(SPI_IMU, IMU_CS_PIN);

// 校准参数
/** 加速度计零偏 */
Vector accBias;
/** 加速度计缩放系数 */
Vector accScale(1, 1, 1);
/** 陀螺仪零偏 */
Vector gyroBias;

/**
 * @brief 初始化IMU模块
 * 
 * 配置SPI引脚并初始化MPU9250传感器。
 */
void setupIMU() {
    SPI_IMU.begin(IMU_SCK_PIN, IMU_MISO_PIN, IMU_MOSI_PIN);

    print("Setup IMU\n");
    imu.begin();
    configureIMU();
}

/**
 * @brief 配置IMU工作参数
 * 
 * 设置加速度计量程、陀螺仪量程、数字低通滤波器和采样率。
 */
void configureIMU() {
    imu.setAccelRange(imu.ACCEL_RANGE_4G);
    imu.setGyroRange(imu.GYRO_RANGE_2000DPS);
    imu.setDLPF(imu.DLPF_MAX);
    imu.setRate(imu.RATE_1KHZ_APPROX);
    imu.setupInterrupt();
}

/**
 * @brief 读取IMU数据
 * 
 * 等待数据就绪后读取陀螺仪和加速度计原始数据，
 * 应用校准参数并转换坐标系。
 */
void readIMU() {
    imu.waitForData();
    imu.getGyro(gyro.x, gyro.y, gyro.z);
    imu.getAccel(acc.x, acc.y, acc.z);
    
    calibrateGyroOnce();
    
    acc = (acc - accBias) / accScale;
    gyro = gyro - gyroBias;
    
    rotateIMU(acc);
    rotateIMU(gyro);
}

/**
 * @brief IMU坐标系转换
 * 
 * 将传感器坐标系转换为飞控标准坐标系。
 * 
 * @param data 待转换的向量
 */
void rotateIMU(Vector& data) {
    data = Vector(data.y, -data.x, data.z);
}

/**
 * @brief 陀螺仪动态校准(仅在着陆时执行)
 * 
 * 当无人机处于静止状态时，持续更新陀螺仪零偏。
 */
void calibrateGyroOnce() {
    static Delay landedDelay(2);
    if (!landedDelay.update(landed)) return;

    static LowPassFilter<Vector> gyroBiasFilter(0.001);
    gyroBias = gyroBiasFilter.update(gyro);
}

/**
 * @brief 加速度计六面校准
 * 
 * 挂起飞行控制任务，按六个方向采集数据并计算校准参数。
 */
void calibrateAccel() {
	vTaskSuspend(flightControlTaskHandle);

	Vector accMax(-INFINITY, -INFINITY, -INFINITY);
	Vector accMin(INFINITY, INFINITY, INFINITY);

	print("校准陀螺仪加速计Calibrating accelerometer\n");
	imu.setAccelRange(imu.ACCEL_RANGE_2G);

	print("1/6 水平放置[8 sec]\n");
	vTaskDelay(pdMS_TO_TICKS(8000)); calibrateAccelOnce(accMax, accMin);
	print("2/6 机头朝上[8 sec]\n");
	vTaskDelay(pdMS_TO_TICKS(8000)); calibrateAccelOnce(accMax, accMin);
	print("3/6 机头朝下[8 sec]\n");
	vTaskDelay(pdMS_TO_TICKS(8000)); calibrateAccelOnce(accMax, accMin);
	print("4/6 右侧朝下[8 sec]\n");
	vTaskDelay(pdMS_TO_TICKS(8000)); calibrateAccelOnce(accMax, accMin);
	print("5/6 左侧朝下[8 sec]\n");
	vTaskDelay(pdMS_TO_TICKS(8000)); calibrateAccelOnce(accMax, accMin);
	print("6/6 倒置放置[8 sec]\n");
	vTaskDelay(pdMS_TO_TICKS(8000)); calibrateAccelOnce(accMax, accMin);

	printIMUCalibration();
	print("✓校准完成Calibration done!\n");
	configureIMU();

	vTaskResume(flightControlTaskHandle);
}

/**
 * @brief 单次加速度计校准采样
 * 
 * 采集指定姿态下的加速度计数据，更新最大值和最小值。
 * 
 * @param accMax 各轴最大值(输出参数)
 * @param accMin 各轴最小值(输出参数)
 */
void calibrateAccelOnce(Vector& accMax, Vector& accMin) {
    const int samples = 1000;

    acc = Vector(0, 0, 0);
    for (int i = 0; i < samples; i++) {
        delay(2);
        Vector sample;
        imu.getAccel(sample.x, sample.y, sample.z);
        acc = acc + sample;
    }
    acc = acc / samples;

    if (acc.x > accMax.x) accMax.x = acc.x;
    if (acc.y > accMax.y) accMax.y = acc.y;
    if (acc.z > accMax.z) accMax.z = acc.z;
    if (acc.x < accMin.x) accMin.x = acc.x;
    if (acc.y < accMin.y) accMin.y = acc.y;
    if (acc.z < accMin.z) accMin.z = acc.z;

    accScale = (accMax - accMin) / 2 / ONE_G;
    accBias = (accMax + accMin) / 2;
}

/**
 * @brief 打印IMU校准参数
 */
void printIMUCalibration() {
    print("gyro bias: %f %f %f\n", gyroBias.x, gyroBias.y, gyroBias.z);
    print("accel bias: %f %f %f\n", accBias.x, accBias.y, accBias.z);
    print("accel scale: %f %f %f\n", accScale.x, accScale.y, accScale.z);
}

/**
 * @brief 打印IMU状态和数据信息
 */
void printIMUInfo() {
    imu.status() ? print("status: ERROR %d\n", imu.status()) : print("status: OK\n");
    print("model: %s\n", imu.getModel());
    print("who am I: 0x%02X\n", imu.whoAmI());
    print("rate: %.0f\n", loopRate);
    print("gyro: %f %f %f\n", rates.x, rates.y, rates.z);
    print("acc: %f %f %f\n", acc.x, acc.y, acc.z);
    
    imu.waitForData();
    Vector rawGyro, rawAcc;
    imu.getGyro(rawGyro.x, rawGyro.y, rawGyro.z);
    imu.getAccel(rawAcc.x, rawAcc.y, rawAcc.z);
    print("raw gyro: %f %f %f\n", rawGyro.x, rawGyro.y, rawGyro.z);
    print("raw acc: %f %f %f\n", rawAcc.x, rawAcc.y, rawAcc.z);
}