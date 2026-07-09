/**
 * format.test.mjs
 *
 * 验证 utils/format.js 中的两个格式化纯函数，确保提取重构后行为与原内联逻辑等价。
 *
 * 测试内容：
 * - formatTimestamp：本地时间 → YYYYMMDDHHmmss 14 位字符串
 *   覆盖正常时间、年初、年末三种边界场景（注意 Date 月份从 0 开始）。
 * - formatFileSize：字节数 → 带单位字符串（B / KB / MB）
 *   覆盖三档分支的边界值（0、500、1023、1024、2048、1048576、2097152）。
 */

import { test } from 'node:test'
import assert from 'node:assert/strict'
import { formatTimestamp, formatFileSize } from '../utils/format.js'

// ==================== formatTimestamp ====================

test('formatTimestamp 正常日期 2026-07-03 10:05:09', () => {
	// Date 构造函数月份从 0 开始，6 表示 7 月
	assert.strictEqual(formatTimestamp(new Date(2026, 6, 3, 10, 5, 9)), '20260703100509')
})

test('formatTimestamp 年初 2026-01-01 00:00:00', () => {
	assert.strictEqual(formatTimestamp(new Date(2026, 0, 1, 0, 0, 0)), '20260101000000')
})

test('formatTimestamp 年末 2025-12-31 23:59:59', () => {
	assert.strictEqual(formatTimestamp(new Date(2025, 11, 31, 23, 59, 59)), '20251231235959')
})

// ==================== formatFileSize ====================

test('formatFileSize 0 字节 → "0 B"', () => {
	assert.strictEqual(formatFileSize(0), '0 B')
})

test('formatFileSize 500 字节 → "500 B"', () => {
	assert.strictEqual(formatFileSize(500), '500 B')
})

test('formatFileSize 1023 字节（B 档上限）→ "1023 B"', () => {
	assert.strictEqual(formatFileSize(1023), '1023 B')
})

test('formatFileSize 1024 字节（KB 档起点）→ "1 KB"', () => {
	assert.strictEqual(formatFileSize(1024), '1 KB')
})

test('formatFileSize 2048 字节 → "2 KB"', () => {
	assert.strictEqual(formatFileSize(2048), '2 KB')
})

test('formatFileSize 1048576 字节（MB 档起点）→ "1.0 MB"', () => {
	assert.strictEqual(formatFileSize(1048576), '1.0 MB')
})

test('formatFileSize 2097152 字节 → "2.0 MB"', () => {
	assert.strictEqual(formatFileSize(2097152), '2.0 MB')
})
