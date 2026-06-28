/**
 * 阿里云视觉智能开放平台 - 物体检测 API 调用工具
 *
 * 流程：截图 → 上传到阿里云OSS获取URL → 调用阿里云DetectObject → 返回结果
 * 纯 JavaScript 实现，不依赖任何原生模块或第三方库
 */

// ==================== 配置信息 ====================
const CONFIG = {
	// AccessKey ID（请替换为你的 AccessKey ID）
	accessKeyId: '填入自己的阿里云AK',
	// AccessKey Secret（请替换为你的 AccessKey Secret）
	accessKeySecret: '填入自己的阿里云SK',
	// 区域
	regionId: 'cn-shanghai',
	// OSS 配置
	oss: {
		bucket: 'esp32-drone-images',
		region: 'cn-shanghai',
		endpoint: 'oss-cn-shanghai.aliyuncs.com',
		folder: 'screenshots'
	}
}

// ==================== 检测模型定义 ====================
// 所有模型共用 DetectObject API（免费），通过客户端过滤实现分类检测
// 如需更高精度，可购买专项 API 后替换 action 和 endpoint
const MODELS = {
	all: {
		key: 'all',
		label: '全部物体',
		action: 'DetectObject',
		endpoint: 'objectdet.cn-shanghai.aliyuncs.com',
		apiVersion: '2019-12-30',
		filter: null // null = 不过滤，显示全部
	},
	person: {
		key: 'person',
		label: '人物',
		action: 'DetectObject',
		endpoint: 'objectdet.cn-shanghai.aliyuncs.com',
		apiVersion: '2019-12-30',
		filter: ['human', 'person', 'people', 'man', 'woman']
	},
	vehicle: {
		key: 'vehicle',
		label: '车辆',
		action: 'DetectObject',
		endpoint: 'objectdet.cn-shanghai.aliyuncs.com',
		apiVersion: '2019-12-30',
		filter: ['car', 'vehicle', 'truck', 'bus', 'bicycle', 'motorcycle', 'bike', 'van', 'suv']
	},
	animal: {
		key: 'animal',
		label: '动物',
		action: 'DetectObject',
		endpoint: 'objectdet.cn-shanghai.aliyuncs.com',
		apiVersion: '2019-12-30',
		filter: ['bird', 'cat', 'dog', 'horse', 'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe', 'animal']
	}
}

// 当前使用的检测模型
let currentModel = 'all'

/**
 * 获取所有可用模型列表
 */
export function getModelList() {
	return Object.values(MODELS).map(m => ({ key: m.key, label: m.label }))
}

/**
 * 切换检测模型
 * @param {string} key - 模型 key
 */
export function setModel(key) {
	if (MODELS[key]) {
		currentModel = key
		console.log('[模型切换] 当前模型:', MODELS[key].label)
		return true
	}
	console.warn('[模型切换] 无效的模型 key:', key)
	return false
}

/**
 * 获取当前模型信息
 */
export function getCurrentModel() {
	return MODELS[currentModel]
}

/**
 * 获取当前模型的过滤关键词列表
 * 返回 null 表示不过滤，显示全部
 */
export function getCurrentFilter() {
	return MODELS[currentModel].filter || null
}

// ==================== 纯JS SHA-1 / HMAC-SHA1 / Base64 实现 ====================

function sha1(str) {
	function rotateLeft(n, s) { return (n << s) | (n >>> (32 - s)) }
	function toHex(val) {
		let hex = ''
		for (let i = 7; i >= 0; i--) hex += ((val >>> (i * 4)) & 0x0F).toString(16)
		return hex
	}
	const K = [0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6]

	const bytes = []
	for (let i = 0; i < str.length; i++) {
		bytes.push(str.charCodeAt(i) & 0xFF)
	}

	const bitLen = bytes.length * 8
	bytes.push(0x80)
	while (bytes.length % 64 !== 56) bytes.push(0)
	bytes.push(0, 0, 0, 0, (bitLen >>> 24) & 0xFF, (bitLen >>> 16) & 0xFF, (bitLen >>> 8) & 0xFF, bitLen & 0xFF)

	let H0 = 0x67452301, H1 = 0xEFCDAB89, H2 = 0x98BADCFE, H3 = 0x10325476, H4 = 0xC3D2E1F0
	const W = new Array(80)

	for (let offset = 0; offset < bytes.length; offset += 64) {
		for (let t = 0; t < 16; t++)
			W[t] = (bytes[offset + t * 4] << 24) | (bytes[offset + t * 4 + 1] << 16) | (bytes[offset + t * 4 + 2] << 8) | bytes[offset + t * 4 + 3]
		for (let t = 16; t < 80; t++) W[t] = rotateLeft(W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16], 1)
		let a = H0, b = H1, c = H2, d = H3, e = H4
		for (let t = 0; t < 80; t++) {
			let f, k
			if (t < 20) { f = (b & c) | (~b & d); k = K[0] }
			else if (t < 40) { f = b ^ c ^ d; k = K[1] }
			else if (t < 60) { f = (b & c) | (b & d) | (c & d); k = K[2] }
			else { f = b ^ c ^ d; k = K[3] }
			const temp = (rotateLeft(a, 5) + f + e + k + W[t]) | 0
			e = d; d = c; c = rotateLeft(b, 30); b = a; a = temp
		}
		H0 = (H0 + a) | 0; H1 = (H1 + b) | 0; H2 = (H2 + c) | 0; H3 = (H3 + d) | 0; H4 = (H4 + e) | 0
	}
	return toHex(H0) + toHex(H1) + toHex(H2) + toHex(H3) + toHex(H4)
}

// 自测 SHA-1
console.log('[加密自测] SHA1("") =', sha1(''), '| 期望: da39a3ee5e6b4b0d3255bfef95601890afd80709')
console.log('[加密自测] SHA1("abc") =', sha1('abc'), '| 期望: a9993e364706816aba3e25717850c26c9cd0d89d')

function hmacSha1Base64(text, key) {
	const BLOCK_SIZE = 64
	const keyBytes = []
	for (let i = 0; i < key.length; i++) keyBytes.push(key.charCodeAt(i) & 0xFF)
	const textBytes = []
	for (let i = 0; i < text.length; i++) textBytes.push(text.charCodeAt(i) & 0xFF)

	let k = keyBytes
	if (k.length > BLOCK_SIZE) {
		const hex = sha1(key)
		k = []
		for (let i = 0; i < hex.length; i += 2) k.push(parseInt(hex.substr(i, 2), 16))
	}

	while (k.length < BLOCK_SIZE) k.push(0)

	const ipad = []
	const opad = []
	for (let i = 0; i < BLOCK_SIZE; i++) {
		ipad.push(k[i] ^ 0x36)
		opad.push(k[i] ^ 0x5C)
	}

	const innerData = ipad.concat(textBytes)
	const innerHashHex = sha1(bytesToBinaryString(innerData))
	const innerHashBytes = []
	for (let i = 0; i < innerHashHex.length; i += 2) innerHashBytes.push(parseInt(innerHashHex.substr(i, 2), 16))

	const outerData = opad.concat(innerHashBytes)
	const outerHashHex = sha1(bytesToBinaryString(outerData))
	const outerHashBytes = []
	for (let i = 0; i < outerHashHex.length; i += 2) outerHashBytes.push(parseInt(outerHashHex.substr(i, 2), 16))

	// 使用 btoa 做 base64 编码（浏览器原生，最可靠）
	const binaryStr = bytesToBinaryString(outerHashBytes)
	return btoa(binaryStr)
}

function bytesToBinaryString(bytes) {
	let result = ''
	for (let i = 0; i < bytes.length; i++) {
		result += String.fromCharCode(bytes[i])
	}
	return result
}

// 自测 HMAC-SHA1
const testHmac = hmacSha1Base64('what do ya want for nothing?', 'Jefe')
console.log('[加密自测] HMAC-SHA1("Jefe", "what..") =', testHmac, '| 期望: 7/zfauXrL6LSdBbV8YTfnCWafHk=')

// ==================== 阿里云签名工具函数 ====================

function signNRandom() { return String(Math.round(Math.random() * 100000000000000)) }

function getTimestamp() {
	const d = new Date()
	const pad = n => String(n).padStart(2, '0')
	return `${d.getUTCFullYear()}-${pad(d.getUTCMonth() + 1)}-${pad(d.getUTCDate())}T${pad(d.getUTCHours())}:${pad(d.getUTCMinutes())}:${pad(d.getUTCSeconds())}Z`
}

function ksort(params) {
	const keys = Object.keys(params).sort()
	const sorted = {}
	keys.forEach(k => { sorted[k] = params[k] })
	return sorted
}

function encode(str) {
	return encodeURIComponent(str).replace(/!/g, '%21').replace(/'/g, '%27').replace(/\(/g, '%28').replace(/\)/g, '%29').replace(/\*/g, '%2A')
}

function objToParam(param) {
	if (!param || typeof param !== 'object') return ''
	let q = ''
	for (const key in param) {
		if (param.hasOwnProperty(key)) {
			q += `&${encode(key)}=${encode(String(param[key]))}`
		}
	}
	return q
}

function getSignature(signedParams, method, secret) {
	const stringToSign = `${method}&${encode('/')}&${encode(signedParams)}`
	return hmacSha1Base64(stringToSign, secret + '&')
}

// ==================== 图片上传（阿里云OSS） ====================

/**
 * 将本地图片上传到阿里云OSS，获取公开访问URL
 * 使用 OSS 表单上传（POST + Policy） + uni.uploadFile
 */
export async function uploadImageToHost(localPath) {
	console.log('[上传] 开始上传图片到OSS...')

	const filename = 'SS_' + Date.now() + '.png'
	const objectKey = `${CONFIG.oss.folder}/${filename}`
	const publicUrl = `https://${CONFIG.oss.bucket}.${CONFIG.oss.endpoint}/${objectKey}`

	// 构造 Policy JSON
	const now = new Date()
	now.setHours(now.getHours() + 1)
	const expiration = now.toISOString()

	const policyObj = {
		expiration: expiration,
		conditions: [
			{ bucket: CONFIG.oss.bucket },
			{ key: objectKey },
			['content-length-range', 0, 10485760]
		]
	}

	// base64 编码 Policy
	const policy = btoa(JSON.stringify(policyObj))

	// 签名：base64(hmac-sha1(AccessKeySecret, policy))
	const signature = hmacSha1Base64(policy, CONFIG.accessKeySecret)

	console.log('[上传] Policy base64:', policy.substring(0, 50) + '...')
	console.log('[上传] Signature:', signature)

	// 使用 uni.uploadFile 进行标准文件上传
	return new Promise((resolve, reject) => {
		uni.uploadFile({
			url: `https://${CONFIG.oss.bucket}.${CONFIG.oss.endpoint}`,
			filePath: localPath,
			name: 'file',
			formData: {
				'key': objectKey,
				'policy': policy,
				'OSSAccessKeyId': CONFIG.accessKeyId,
				'Signature': signature,
				'success_action_status': '200'
			},
			timeout: 30000,
			success: (res) => {
				console.log('[上传] OSS响应状态:', res.statusCode)
				if (res.statusCode === 200) {
					console.log('[上传] OSS上传成功:', publicUrl)
					// 验证：等1秒后检查文件是否存在
					setTimeout(() => {
						uni.request({
							url: publicUrl,
							method: 'HEAD',
							timeout: 5000,
							success: (checkRes) => {
								console.log('[上传] 文件验证:', checkRes.statusCode)
							}
						})
					}, 1000)
					resolve(publicUrl)
				} else {
					const msg = typeof res.data === 'string' ? res.data.substring(0, 500) : JSON.stringify(res.data)
					console.error('[上传] OSS上传失败:', res.statusCode, msg)
					reject(new Error('OSS上传失败: HTTP ' + res.statusCode))
				}
			},
			fail: (err) => {
				console.error('[上传] OSS请求失败:', err)
				reject(err)
			}
		})
	})
}

// ==================== 阿里云物体检测API ====================

/**
 * 调用阿里云物体检测 API（使用短URL，避免414）
 * @param {string} imageUrl - 图片的公开可访问URL（短链接）
 * @returns {Promise<Object>} - 检测结果
 */
export async function detectObjects(imageUrl) {
	try {
		// 使用当前模型的配置
		const model = MODELS[currentModel]
		const endpoint = model.endpoint
		const action = model.action
		const apiVersion = model.apiVersion

		// 构建请求参数（ImageURL 是短链接，不会超长）
		const request_ = {
			'SignatureMethod': 'HMAC-SHA1',
			'SignatureNonce': signNRandom(),
			'AccessKeyId': CONFIG.accessKeyId,
			'SignatureVersion': '1.0',
			'Timestamp': getTimestamp(),
			'Format': 'JSON',
			'RegionId': CONFIG.regionId,
			'Version': apiVersion,
			'Action': action,
			'ImageURL': imageUrl
		}

		// 计算签名
		const sortParams = ksort(request_)
		const sortedQueryString = objToParam(sortParams).substring(1)
		const Signature = getSignature(sortedQueryString, 'POST', CONFIG.accessKeySecret)
		request_['Signature'] = Signature

		// 所有参数放入 URL 查询字符串（ImageURL是短链接，不会触发414）
		const fullUrl = 'https://' + endpoint + '/?' + objToParam(request_).substring(1)

		console.log(`[阿里云检测] 模型: ${model.label}, 发起请求...`)
		console.log('[阿里云检测] 请求URL长度:', fullUrl.length)
		console.log('[阿里云检测] 请求URL前500:', fullUrl.substring(0, 500))

		return new Promise((resolve, reject) => {
			uni.request({
				url: fullUrl,
				method: 'POST',
				header: { 'Content-Type': 'application/x-www-form-urlencoded' },
				timeout: 20000,
				success: (res) => {
					console.log('[阿里云检测] 响应状态:', res.statusCode)
					// 打印完整响应数据
					const rawData = typeof res.data === 'string' ? res.data : JSON.stringify(res.data)
					console.log('[阿里云检测] 原始响应:', rawData)
					if (res.statusCode === 200) {
						resolve(res.data)
					} else {
						reject(new Error(`HTTP ${res.statusCode}: ${rawData.substring(0, 500)}`))
					}
				},
				fail: (err) => {
					console.error('[阿里云检测] 请求失败:', err)
					reject(err)
				}
			})
		})
	} catch (error) {
		console.error('[阿里云检测] 异常:', error)
		throw error
	}
}

/**
 * 一站式方法：上传本地图片 → 阿里云检测 → 返回结果
 * @param {string} localImagePath - 本地截图文件路径
 * @returns {Promise<Object>} - 检测结果
 */
export async function detectLocalImage(localImagePath) {
	const model = MODELS[currentModel]
	console.log(`[检测流程] 模型: ${model.label}, 图片已上传，URL长度:`, 0)

	// 步骤1：上传本地图片到图床，获取短URL
	const imageUrl = await uploadImageToHost(localImagePath)
	console.log(`[检测流程] 图片已上传，URL长度:`, imageUrl.length)

	// 步骤2：使用短URL调用阿里云检测
	const result = await detectObjects(imageUrl)
	return result
}

// ==================== 结果解析 ====================

/**
 * 解析检测结果
 */
export function parseDetectionResult(response) {
	console.log('[解析] 原始响应结构:', JSON.stringify(response, null, 2))

	if (!response || response.Code) {
		console.error('[阿里云检测] API错误:', response?.Code, response?.Message)
		return []
	}

	const objects = []

	// DetectObject API 返回格式：Data.Elements
	if (response.Data && response.Data.Elements) {
		const elements = Array.isArray(response.Data.Elements) ? response.Data.Elements : [response.Data.Elements]
		elements.forEach((item) => {
			objects.push({
				name: item.Type || '未知物体',
				score: item.Score || 0,
				box: {
					left: item.Boxes ? item.Boxes[0] || 0 : 0,
					top: item.Boxes ? item.Boxes[1] || 0 : 0,
					width: item.Boxes ? (item.Boxes[2] || 0) - (item.Boxes[0] || 0) : 0,
					height: item.Boxes ? (item.Boxes[3] || 0) - (item.Boxes[1] || 0) : 0
				}
			})
		})
	}

	// 兼容旧格式：Data.Data (某些API可能返回)
	if (objects.length === 0 && response.Data && response.Data.Data) {
		const dataList = Array.isArray(response.Data.Data) ? response.Data.Data : [response.Data.Data]
		dataList.forEach((item) => {
			objects.push({
				name: item.Name || '未知物体',
				score: item.Score || 0,
				box: {
					left: item.Box ? item.Box.left || 0 : 0,
					top: item.Box ? item.Box.top || 0 : 0,
					width: item.Box ? item.Box.width || 0 : 0,
					height: item.Box ? item.Box.height || 0 : 0
				}
			})
		})
	}

	console.log(`[阿里云检测] 检测到 ${objects.length} 个物体`)
	if (objects.length > 0) {
		console.log('[阿里云检测] 检测结果:', JSON.stringify(objects))
	}

	return objects
}

export { CONFIG }
