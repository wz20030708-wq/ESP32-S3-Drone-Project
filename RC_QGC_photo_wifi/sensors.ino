/**
 * @file sensors.ino
 * @brief I2C传感器模块(PM280气压传感器 + VL53L0X激光测距)
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 通过I2C总线驱动PM280(BMP280兼容)气压传感器和VL53L0X激光测距传感器。
 * PM280用于测量气压和温度，VL53L0X用于障碍物检测。
 * 
 * 硬件连接(ESP32-S3):
 *   SDA → GPIO47
 *   SCL → GPIO21
 *   VCC → 3.3V
 *   GND → GND
 */

#include <Wire.h>
#include <VL53L0X.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// ====================== I2C 引脚配置 ======================

/** I2C数据线引脚 */
#define I2C_SDA_PIN  47
/** I2C时钟线引脚 */
#define I2C_SCL_PIN  21
/** I2C时钟频率(400kHz Fast Mode) */
#define I2C_CLOCK    400000

// ====================== 传感器 I2C 地址 ======================

/** PM280/BMP280地址(SDO=GND) */
#define PM280_ADDR    0x76
/** PM280/BMP280备选地址(SDO=VDD) */
#define PM280_ALT_ADDR 0x77
/** VL53L0X固定地址 */
#define VL53L0X_ADDR  0x29

// ====================== 传感器状态标志 ======================

/** PM280初始化成功标志 */
bool pm280Ok    = false;
/** VL53L0X初始化成功标志 */
bool vl53l0xOk  = false;

/** VL53L0X库对象 */
VL53L0X vl53l0x;

// ====================== 全局测量数据 ======================

/** 气压值(hPa) */
float pressureHpa = 0.0f;
/** 距离值(mm) */
float distanceMm  = 0.0f;
/** 海拔高度(m) */
float altitudeM   = 0.0f;
/** PM280温度(°C) */
float temperatureC = 0.0f;

/** 海平面参考气压(hPa) */
#define SEA_LEVEL_PRESSURE 1010.25f

// ====================== PM280 校准系数 ======================

/** 温度补偿参数 */
uint16_t dig_T1;
int16_t  dig_T2, dig_T3;

/** 气压补偿参数 */
uint16_t dig_P1;
int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;

/** 温度精细值(供气压补偿使用) */
int32_t t_fine;

// ====================== I2C 底层读写 ======================

/**
 * @brief 向指定I2C设备写入一个寄存器值
 * @param addr 设备地址
 * @param reg 寄存器地址
 * @param value 待写入的值
 */
void i2cWrite8(uint8_t addr, uint8_t reg, uint8_t value) {
	Wire.beginTransmission(addr);
	Wire.write(reg);
	Wire.write(value);
	Wire.endTransmission();
}

/**
 * @brief 从指定I2C设备读取一个字节
 * @param addr 设备地址
 * @param reg 寄存器地址
 * @return 读取的字节值
 */
uint8_t i2cRead8(uint8_t addr, uint8_t reg) {
	Wire.beginTransmission(addr);
	Wire.write(reg);
	Wire.endTransmission(false);
	Wire.requestFrom(addr, (uint8_t)1);
	return Wire.read();
}

/**
 * @brief 从指定I2C设备读取连续多个字节
 * @param addr 设备地址
 * @param reg 起始寄存器地址
 * @param buf 接收缓冲区
 * @param len 读取字节数
 */
void i2cReadBuf(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len) {
	Wire.beginTransmission(addr);
	Wire.write(reg);
	Wire.endTransmission(false);
	Wire.requestFrom(addr, len);
	for (uint8_t i = 0; i < len; i++) {
		buf[i] = Wire.read();
	}
}

// ====================== PM280 气压传感器 ======================

/**
 * @brief 读取PM280校准系数
 * 
 * 从寄存器0x88和0xE1读取温度和气压补偿参数。
 */
void pm280ReadCalibration() {
	uint8_t buf[24];
	i2cReadBuf(PM280_ADDR, 0x88, buf, 24);

	dig_T1 = (uint16_t)(buf[0]  | (buf[1]  << 8));
	dig_T2 = (int16_t)( buf[2]  | (buf[3]  << 8));
	dig_T3 = (int16_t)( buf[4]  | (buf[5]  << 8));
	dig_P1 = (uint16_t)(buf[6]  | (buf[7]  << 8));
	dig_P2 = (int16_t)( buf[8]  | (buf[9]  << 8));
	dig_P3 = (int16_t)( buf[10] | (buf[11] << 8));
	dig_P4 = (int16_t)( buf[12] | (buf[13] << 8));
	dig_P5 = (int16_t)( buf[14] | (buf[15] << 8));
	dig_P6 = (int16_t)( buf[16] | (buf[17] << 8));
	dig_P7 = (int16_t)( buf[18] | (buf[19] << 8));
	dig_P8 = (int16_t)( buf[20] | (buf[21] << 8));
	dig_P9 = (int16_t)( buf[22] | (buf[23] << 8));
}

/**
 * @brief PM280温度补偿计算
 * @param adc_T 温度原始ADC值
 * @return 温度×100(单位:0.01°C)
 */
int32_t pm280CompensateTemperature(int32_t adc_T) {
	int32_t var1, var2, T;

	var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
	var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12)
		* ((int32_t)dig_T3)) >> 14;

	t_fine = var1 + var2;
	T = (t_fine * 5 + 128) >> 8;
	return T;
}

/**
 * @brief PM280气压补偿计算
 * @param adc_P 气压原始ADC值
 * @return 气压×256(单位:Pa)
 */
uint32_t pm280CompensatePressure(int32_t adc_P) {
	int64_t var1, var2, p;

	var1 = ((int64_t)t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)dig_P6;
	var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
	var2 = var2 + (((int64_t)dig_P4) << 35);
	var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
	var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;

	if (var1 == 0) {
		return 0;
	}

	p = 1048576 - adc_P;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((int64_t)dig_P8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
	return (uint32_t)p;
}

/**
 * @brief PM280传感器初始化
 * @return true表示初始化成功，false表示失败
 */
bool pm280Init() {
	uint8_t chipId;

	Wire.beginTransmission(PM280_ADDR);
	if (Wire.endTransmission() != 0) {
		Wire.beginTransmission(PM280_ALT_ADDR);
		if (Wire.endTransmission() != 0) {
			print("PM280: 未检测到设备 (I2C 无应答)\n");
			return false;
		}
		print("PM280: 仅支持地址 0x76 (SDO 接 GND)\n");
		return false;
	}

	chipId = i2cRead8(PM280_ADDR, 0xD0);
	if (chipId != 0x58) {
		print("PM280: 芯片 ID 不匹配 (期望 0x58, 实际 0x%02X)\n", chipId);
		return false;
	}

	pm280ReadCalibration();

	i2cWrite8(PM280_ADDR, 0xF4, 0xB5);
	i2cWrite8(PM280_ADDR, 0xF5, 0x10);

	print("PM280: 初始化成功, 芯片 ID=0x58\n");
	return true;
}

/**
 * @brief 读取PM280气压值
 * @return 气压值(hPa)，失败返回0
 */
float pm280ReadPressure() {
	if (!pm280Ok) return 0.0f;

	uint8_t buf[6];
	i2cReadBuf(PM280_ADDR, 0xF7, buf, 6);

	int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
	int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);

	int32_t temp_x100 = pm280CompensateTemperature(adc_T);
	temperatureC = temp_x100 / 100.0f;

	uint32_t pressPa_x256 = pm280CompensatePressure(adc_P);
	float pressure = (float)pressPa_x256 / 25600.0f;

	if (pressure < 300.0f || pressure > 1100.0f) {
		return 0.0f;
	}
	return pressure;
}

// ====================== VL53L0X 激光测距传感器 ======================

/**
 * @brief VL53L0X传感器初始化
 * @return true表示初始化成功，false表示失败
 */
bool vl53l0xInit() {
	Wire.beginTransmission(VL53L0X_ADDR);
	if (Wire.endTransmission() != 0) {
		print("VL53L0X: I2C 无应答 (硬件未检测到)\n");
		return false;
	}
	uint8_t id = i2cRead8(VL53L0X_ADDR, 0xC0);
	print("VL53L0X: 检测到芯片 ModelID=0x%02X\n", id);

	if (!vl53l0x.init()) {
		print("VL53L0X: 库 init() 失败 (可能是校准时序问题)\n");
		return false;
	}
	vl53l0x.setTimeout(500);
	vl53l0x.startContinuous();
	print("VL53L0X: 初始化+连续测距成功\n");
	return true;
}

/**
 * @brief 读取VL53L0X距离值
 * @return 距离(mm)，失败返回-1
 */
float vl53l0xReadDistance() {
	if (!vl53l0xOk) return -1.0f;

	uint16_t dist = vl53l0x.readRangeContinuousMillimeters();
	if (vl53l0x.timeoutOccurred() || dist == 65535) {
		return -1.0f;
	}

	if (dist < 80 || dist > 1000) {
		return -1.0f;
	}
	return (float)dist;
}

// ====================== 统一传感器接口 ======================

/**
 * @brief 初始化所有I2C传感器
 */
void setupSensors() {
	print("初始化 I2C 传感器 (SDA=47, SCL=21)\n");

	Wire.end();
	delay(5);
	Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

	vl53l0xOk = vl53l0xInit();
	pm280Ok = pm280Init();

	print("I2C 扫描: ");
	for (uint8_t addr = 1; addr < 127; addr++) {
		Wire.beginTransmission(addr);
		if (Wire.endTransmission() == 0) {
			print("0x%02X ", addr);
		}
	}
	print("\n");

	if (pm280Ok)   print("PM280:    气压传感器就绪\n");
	if (vl53l0xOk) print("VL53L0X:  激光测距传感器就绪\n");
	if (!pm280Ok && !vl53l0xOk) print("警告: 未检测到任何 I2C 传感器\n");
}

/**
 * @brief 读取所有传感器数据
 */
void readSensors() {
	if (pm280Ok) {
		float p = pm280ReadPressure();
		if (p > 0) {
			pressureHpa = p;
			altitudeM = 44330.0f * (1.0f - powf(p / SEA_LEVEL_PRESSURE, 1.0f / 5.255f));
		}
	}

	if (vl53l0xOk) {
		float d = vl53l0xReadDistance();
		if (d >= 0) distanceMm = d;
	}
}

/**
 * @brief 打印传感器数据(调试用)
 */
void printSensors() {
	if (pm280Ok) {
		print("气压: %.1f hPa  温度: %.1f °C  海拔: %.1f m\n", pressureHpa, temperatureC, altitudeM);
	} else {
		print("气压: 未连接\n");
	}
	if (vl53l0xOk) {
		print("距离: %.0f mm\n", distanceMm);
	} else {
		print("距离: 未连接\n");
	}
}