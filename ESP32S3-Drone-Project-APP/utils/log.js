/**
 * 轻量级日志工具
 * 提供可控的分级日志输出，替代散落的 console.log 调试调用。
 * debug 级别受全局开关 DEBUG 控制（默认关闭，生产环境无日志 IO）；
 * info/warn/error 始终输出，用于关键状态与错误追踪。
 */

// 调试开关：调试时改为 true 开启 debug 级日志；生产环境保持 false
const DEBUG = false

export const log = {
	/** debug 级日志，仅当 DEBUG 为 true 时输出（用于详细调试信息） */
	debug(...args) {
		if (DEBUG) console.log(...args)
	},
	/** info 级日志，始终输出（用于关键流程状态） */
	info(...args) {
		console.log(...args)
	},
	/** warn 级日志，始终输出（用于警告） */
	warn(...args) {
		console.warn(...args)
	},
	/** error 级日志，始终输出（用于错误） */
	error(...args) {
		console.error(...args)
	}
}
