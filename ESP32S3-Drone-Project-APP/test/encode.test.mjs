/**
 * encode.test.mjs
 *
 * 验证 utils/aliyun-api.js 中的 encode 函数实现 RFC3986 编码，
 * 行为与原 5 次链式 replace 等价（现实现为单次正则 + 映射表）。
 *
 * 测试内容：
 * - 空串
 * - 空格编码
 * - 5 个 RFC3986 保留字符：! * ' ( )
 * - 斜杠 /
 * - 中文 UTF-8 编码
 * - = 与 & 编码
 */

import { test } from 'node:test'
import assert from 'node:assert/strict'
import { encode } from '../utils/aliyun-api.js'

test('encode 空字符串', () => {
	assert.strictEqual(encode(''), '')
})

test('encode 空格 → %20', () => {
	assert.strictEqual(encode('hello world'), 'hello%20world')
})

test("encode 5 个 RFC3986 保留字符 ! * ' ( ) 全部编码", () => {
	assert.strictEqual(encode("!*'()"), '%21%2A%27%28%29')
})

test('encode 斜杠 / → %2F', () => {
	assert.strictEqual(encode('a/b'), 'a%2Fb')
})

test('encode 中文 UTF-8 编码', () => {
	assert.strictEqual(encode('测试'), '%E6%B5%8B%E8%AF%95')
})

test('encode 等号与 & 编码', () => {
	assert.strictEqual(encode('a=b&c=d'), 'a%3Db%26c%3Dd')
})
