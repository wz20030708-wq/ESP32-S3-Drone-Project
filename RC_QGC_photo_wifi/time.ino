/**
 * @file time.ino
 * @brief 时间管理模块
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 提供高精度时间计算和循环频率统计功能。
 */

/** 主循环频率(Hz) */
float loopRate;

/**
 * @brief 更新时间戳
 * 
 * 计算当前时间和时间间隔(dt)，并更新循环频率。
 */
void step() {
	float now = micros() / 1000000.0;
	dt = now - t;
	t = now;

	if (!(dt > 0)) {
		dt = 0;
	}

	computeLoopRate();
}

/**
 * @brief 计算主循环频率
 * 
 * 每秒统计一次主循环执行次数，用于监控系统性能。
 */
void computeLoopRate() {
	static float windowStart = 0;
	static uint32_t rate = 0;
	rate++;
	if (t - windowStart >= 1) {
		loopRate = rate;
		windowStart = t;
		rate = 0;
	}
}