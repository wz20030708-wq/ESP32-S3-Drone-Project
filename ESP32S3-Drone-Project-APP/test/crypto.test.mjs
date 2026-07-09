/**
 * crypto.test.mjs
 *
 * 验证 utils/aliyun-api.js 中的 SHA-1 与 HMAC-SHA1（Base64）纯函数实现。
 * 使用的标准向量与原模块加载时的自检向量一致，确保代码优化前后行为等价。
 *
 * 测试内容：
 * - sha1：空串、'abc'、经典英文句子三个 RFC 标准向量
 * - hmacSha1Base64：RFC 2202 经典向量（"Jefe" 密钥）
 */

import { test } from 'node:test'
import assert from 'node:assert/strict'
import { sha1, hmacSha1Base64 } from '../utils/aliyun-api.js'

test('sha1 空字符串返回标准空串哈希', () => {
	assert.strictEqual(sha1(''), 'da39a3ee5e6b4b0d3255bfef95601890afd80709')
})

test('sha1 "abc" 返回标准向量值', () => {
	assert.strictEqual(sha1('abc'), 'a9993e364706816aba3e25717850c26c9cd0d89d')
})

test('sha1 经典英文句子返回标准向量值', () => {
	assert.strictEqual(sha1('The quick brown fox jumps over the lazy dog'), '2fd4e1c67a2d28fced849ee1bb76e7391b93eb12')
})

test('hmacSha1Base64 RFC 2202 经典向量（Jefe 密钥）', () => {
	assert.strictEqual(
		hmacSha1Base64('what do ya want for nothing?', 'Jefe'),
		'7/zfauXrL6LSdBbV8YTfnCWafHk='
	)
})
