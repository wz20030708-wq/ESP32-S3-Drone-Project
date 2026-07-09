/**
 * @file log.ino
 * @brief RAM日志记录模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 在RAM中循环记录飞行数据，支持实时查看和导出。
 * 记录内容包括姿态、角速度、目标值等关键飞行参数。
 */

#include "vector.h"
#include "util.h"

/** 日志记录频率(Hz) */
#define LOG_RATE 100
/** 日志记录时长(秒) */
#define LOG_DURATION 10
/** 日志总条目数 */
#define LOG_SIZE LOG_DURATION * LOG_RATE

/** 当前姿态的欧拉角表示 */
Vector attitudeEuler;
/** 目标姿态的欧拉角表示 */
Vector attitudeTargetEuler;

/**
 * @brief 日志条目结构体
 */
struct LogEntry {
	const char *name;  /**< 条目名称 */
	float *value;      /**< 值指针 */
};

/** 日志条目定义 */
LogEntry logEntries[] = {
	{"t", &t},
	{"rates.x", &rates.x},
	{"rates.y", &rates.y},
	{"rates.z", &rates.z},
	{"ratesTarget.x", &ratesTarget.x},
	{"ratesTarget.y", &ratesTarget.y},
	{"ratesTarget.z", &ratesTarget.z},
	{"attitude.x", &attitudeEuler.x},
	{"attitude.y", &attitudeEuler.y},
	{"attitude.z", &attitudeEuler.z},
	{"attitudeTarget.x", &attitudeTargetEuler.x},
	{"attitudeTarget.y", &attitudeTargetEuler.y},
	{"attitudeTarget.z", &attitudeTargetEuler.z},
	{"thrustTarget", &thrustTarget}
};

/** 日志列数 */
const int logColumns = sizeof(logEntries) / sizeof(logEntries[0]);
/** 日志缓冲区 */
float logBuffer[LOG_SIZE][logColumns];
/** 当前写入位置 */
int logPointer = 0;
/** 总记录数 */
int logTotal = 0;

/**
 * @brief 准备日志数据
 * 
 * 将四元数姿态转换为欧拉角格式。
 */
void prepareLogData() {
	attitudeEuler = attitude.toEuler();
	attitudeTargetEuler = attitudeTarget.toEuler();
}

/**
 * @brief 记录日志数据
 * 
 * 在解锁状态下按设定频率记录数据，锁定时清空缓冲区。
 */
void logData() {
	if (!armed) {
		if (logPointer != 0 || logTotal != 0) {
			memset(logBuffer, 0, sizeof(logBuffer));
			logPointer = 0;
			logTotal = 0;
		}
		return;
	}
	static Rate period(LOG_RATE);
	if (!period) return;

	prepareLogData();

	for (int i = 0; i < logColumns; i++) {
		logBuffer[logPointer][i] = *logEntries[i].value;
	}

	logPointer++;
	logTotal++;
	if (logPointer >= LOG_SIZE) {
		logPointer = 0;
	}
}

/**
 * @brief 打印日志表头
 */
void printLogHeader() {
	for (int i = 0; i < logColumns; i++) {
		print("%s%s", logEntries[i].name, i < logColumns - 1 ? "," : "\n");
	}
}

/**
 * @brief 打印日志数据
 * 
 * 按时间顺序输出完整日志内容。
 */
void printLogData() {
	int count = logTotal < LOG_SIZE ? logTotal : LOG_SIZE;
	int start = logTotal < LOG_SIZE ? 0 : logPointer;

	for (int i = 0; i < count; i++) {
			int idx = (start + i) % LOG_SIZE;
		for (int j = 0; j < logColumns; j++) {
			print("%g%s", logBuffer[idx][j], j < logColumns - 1 ? "," : "\n");
		}
	}
}