/**
 * parse.test.mjs
 *
 * 验证 utils/aliyun-api.js 中的 parseDetectionResult 函数，
 * 确保能正确解析阿里云 DetectObject 响应并兼容各类异常输入。
 *
 * 测试内容：
 * - 标准 DetectObject 响应（单元素，验证 box 宽高计算）
 * - 空响应（null）
 * - API 错误响应（含 Code 字段）
 * - 缺失 Data 字段
 * - 多元素响应
 */

import { test } from 'node:test'
import assert from 'node:assert/strict'
import { parseDetectionResult } from '../utils/aliyun-api.js'

test('标准 DetectObject 响应：返回单元素，box 宽高正确计算', () => {
	const result = parseDetectionResult({
		Data: {
			Elements: [{ Type: 'person', Score: 0.95, Boxes: [10, 20, 110, 220] }]
		}
	})
	assert.strictEqual(result.length, 1)
	assert.deepStrictEqual(result[0], {
		name: 'person',
		score: 0.95,
		box: { left: 10, top: 20, width: 100, height: 200 }
	})
})

test('空响应 null → 返回空数组', () => {
	assert.deepStrictEqual(parseDetectionResult(null), [])
})

test('API 错误响应（含 Code）→ 返回空数组', () => {
	assert.deepStrictEqual(
		parseDetectionResult({ Code: 'InvalidAccessKeyId', Message: 'xxx' }),
		[]
	)
})

test('无 Data 字段 → 返回空数组', () => {
	assert.deepStrictEqual(parseDetectionResult({}), [])
})

test('多元素响应：返回长度 2', () => {
	const result = parseDetectionResult({
		Data: {
			Elements: [
				{ Type: 'cat', Score: 0.8, Boxes: [0, 0, 50, 50] },
				{ Type: 'dog', Score: 0.7, Boxes: [60, 60, 100, 100] }
			]
		}
	})
	assert.strictEqual(result.length, 2)
})
