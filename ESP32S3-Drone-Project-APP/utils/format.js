/**
 * 通用格式化工具模块
 *
 * 提取自 webview.vue 与 recordings.vue 中重复的内联格式化逻辑，
 * 封装为共享纯函数供多个页面复用。行为与原内联逻辑逐字符等价。
 *
 * 包含：
 * - formatTimestamp：本地时间戳格式化为 YYYYMMDDHHmmss
 * - formatFileSize：字节数格式化为带单位字符串（B/KB/MB）
 */

/**
 * 格式化时间戳为 14 位连续数字字符串
 *
 * 严格复刻 webview.vue 中 _saveBitmap 与 _drawAnnotations 的内联逻辑：
 * 使用本地时间方法（getFullYear/getMonth/getDate/getHours/getMinutes/getSeconds），
 * 月份 +1，对月/日/时/分/秒用 String(n).padStart(2, '0') 补零。
 *
 * @param {Date} date - 日期对象
 * @returns {string} 形如 YYYYMMDDHHmmss 的 14 位字符串
 */
export function formatTimestamp(date) {
	return date.getFullYear() +
		String(date.getMonth() + 1).padStart(2, '0') +
		String(date.getDate()).padStart(2, '0') +
		String(date.getHours()).padStart(2, '0') +
		String(date.getMinutes()).padStart(2, '0') +
		String(date.getSeconds()).padStart(2, '0')
}

/**
 * 格式化文件大小为带单位字符串
 *
 * 严格复刻 recordings.vue 中 loadFiles 的内联三档分支逻辑：
 * - < 1024：直接显示字节数 + ' B'
 * - < 1024 * 1024：Math.round(bytes / 1024) + ' KB'（四舍五入）
 * - >= 1024 * 1024：(bytes / (1024 * 1024)).toFixed(1) + ' MB'（保留 1 位小数）
 *
 * @param {number} bytes - 文件字节数
 * @returns {string} 带单位的大小字符串（B / KB / MB）
 */
export function formatFileSize(bytes) {
	if (bytes < 1024) {
		return bytes + ' B'
	} else if (bytes < 1024 * 1024) {
		return Math.round(bytes / 1024) + ' KB'
	} else {
		return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
	}
}
