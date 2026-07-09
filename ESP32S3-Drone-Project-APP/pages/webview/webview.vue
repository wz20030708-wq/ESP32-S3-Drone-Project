<template>
	<view class="webview-container">
		<web-view 
			:src="webviewUrl"
			@error="onWebviewError"
		></web-view>

		<!-- 模型选择器（cover-view 覆盖在 web-view 上方） -->
		<cover-view class="model-selector">
			<cover-view class="model-btn" @tap="showModelPicker = !showModelPicker">
				<cover-view class="model-current">{{ currentModelLabel }} ▼</cover-view>
			</cover-view>
			<cover-view class="model-dropdown" v-if="showModelPicker">
				<cover-view 
					class="model-option" 
					:class="{ 'model-option-active': item.key === currentModel }"
					v-for="item in modelList" 
					:key="item.key"
					@tap="switchModel(item.key)"
				>
					{{ item.label }}
					<cover-view class="model-check" v-if="item.key === currentModel">✓</cover-view>
				</cover-view>
			</cover-view>
		</cover-view>

		<!-- 检测状态提示 -->
		<view class="detect-toast" v-if="isDetecting">
			<view class="detect-loading"></view>
			<text class="detect-text">AI 识别中...</text>
		</view>

		<!-- 物体检测警告弹窗：检测到物体时自动弹出（使用 cover-view 覆盖 web-view） -->
		<cover-view class="alert-popup" v-if="showAlertPopup && detectionResults.length > 0">
			<cover-view class="alert-popup-header">
				<cover-view class="alert-popup-icon">&#x26A0;</cover-view>
				<cover-view class="alert-popup-title">检测到 {{ detectionResults.length }} 个物体</cover-view>
				<cover-view class="alert-popup-close" @tap="showAlertPopup = false">&times;</cover-view>
			</cover-view>
			<cover-view class="alert-popup-body">
				<cover-view class="alert-item" v-for="(item, index) in detectionResults" :key="index">
					<cover-view class="alert-item-name">{{ item.name }}</cover-view>
					<cover-view class="alert-item-score">{{ (item.score * 100).toFixed(1) }}%</cover-view>
				</cover-view>
			</cover-view>
		</cover-view>

		<!-- 检测结果浮窗 -->
		<view class="result-popup" v-if="showResultPopup" @tap="showResultPopup = false">
			<view class="result-content" @tap.stop>
				<view class="result-header">
					<text class="result-title">检测结果</text>
					<text class="result-close" @tap="showResultPopup = false">&times;</text>
				</view>
				<!-- 标注图片预览 -->
				<view class="result-image-wrap" v-if="annotatedImageSrc">
					<image class="result-image" :src="annotatedImageSrc" mode="widthFix"></image>
				</view>
				<scroll-view scroll-y class="result-list">
					<view class="result-item" v-for="(item, index) in detectionResults" :key="index">
						<text class="item-name">{{ item.name }}</text>
						<text class="item-score">{{ (item.score * 100).toFixed(1) }}%</text>
					</view>
					<view v-if="detectionResults.length === 0" class="no-result">
						<text>未检测到物体</text>
					</view>
				</scroll-view>
			</view>
		</view>

		<!-- 右下角浮层：实时标注图片（cover-view 覆盖原生 web-view） -->
		<cover-view class="floating-annotated" v-if="showFloatingImage && floatingImageSrc">
			<cover-view class="floating-close" @tap="showFloatingImage = false">×</cover-view>
			<cover-image class="floating-image" :src="floatingImageSrc"></cover-image>
			<cover-view class="floating-label" v-if="lastError">{{ lastError }}</cover-view>
		</cover-view>
	</view>
</template>

<script>
	import { detectLocalImage, parseDetectionResult, getModelList, setModel, getCurrentModel, getCurrentFilter } from '@/utils/aliyun-api.js'
	import { log } from '@/utils/log.js'
	import { formatTimestamp } from '@/utils/format.js'

	// 导航栏按钮预定义配置，避免每次截图都重建对象
	const NAV_BTN_DISCONNECT = {
		text: '\u2715 \u65ad\u5f00',
		fontSize: '13px',
		color: '#ffffff',
		background: 'rgba(220,38,38,0.88)',
		float: 'left',
		width: '68px',
		fontWeight: 'bold'
	}
	const NAV_BTN_SCREENSHOT = {
		text: '',
		fontSize: '13px',
		color: '#ffffff',
		background: 'rgba(37,99,235,0.88)',
		float: 'left',
		width: '90px',
		fontWeight: 'bold'
	}

	// Android 原生类缓存：首次调用 _drawAnnotations 时导入并缓存，避免每轮检测重复 JNI 导入
	let nativeClassCache = null

	// 截图裁剪区域（避开导航栏等 UI 元素），数值与原硬编码一致
	const SCREENSHOT_CLIP = { top: '120px', left: '10px', width: '620px', height: '600px' }

	export default {
		data() {
			return {
				webviewUrl: '',
				deviceIp: '',
				screenshotTimer: null,
				screenshotCount: 0,
				isCapturing: false,
				_wasCapturing: false,
				// 阿里云检测相关
				isDetecting: false,
			showResultPopup: false,
			showAlertPopup: false, // 物体检测警告弹窗
				detectionResults: [],
				lastScreenshotPath: '',
				lastAnnotatedPath: '',
				annotatedImageSrc: '', // 标注图片的 data URL，用于直接显示
				showFloatingImage: false, // 右下角浮层显示
				floatingImageSrc: '', // 浮层标注图片
				enableAutoDetect: true, // 是否自动检测
				_readyTimer: null, // onReady 延迟定时器
				lastError: '', // 最近一次检测错误信息
				// 模型选择
				modelList: [], // 可用模型列表
				currentModel: 'all', // 当前模型 key
				showModelPicker: false // 是否显示模型选择面板
			}
		},
		computed: {
			currentModelLabel() {
				const m = this.modelList.find(item => item.key === this.currentModel)
				return m ? m.label : '全部物体'
			}
		},
		onLoad(options) {
			// 接收从主页传递的 IP 参数
			if (options.ip) {
				this.deviceIp = decodeURIComponent(options.ip)
				this.webviewUrl = 'http://' + this.deviceIp
			} else {
				// 默认 IP
				this.deviceIp = '192.168.4.1'
				this.webviewUrl = 'http://' + this.deviceIp
			}
			// 初始化模型列表
			this.modelList = getModelList()
			this.currentModel = getCurrentModel().key
		},
		onReady() {
			// 页面渲染完成后，延迟启动截图（等待 web-view 加载）
			// #ifdef APP-PLUS
			this._readyTimer = setTimeout(() => {
				this._readyTimer = null
				this.startScreenshotCapture()
			}, 3000)
			// #endif
		},
		onShow() {
			// 从其他页面返回时恢复截图
			// #ifdef APP-PLUS
			if (this._wasCapturing && !this.isCapturing) {
				this.startScreenshotCapture()
			}
			// #endif
		},
		onHide() {
			// 跳转到其他页面时暂停截图
			// #ifdef APP-PLUS
			this._wasCapturing = this.isCapturing
			this.pauseScreenshotCapture()
			// #endif
		},
		onUnload() {
			if (this._readyTimer) {
				clearTimeout(this._readyTimer)
				this._readyTimer = null
			}
			this.stopScreenshotCapture()
		},
		// 原生导航栏按钮点击
		onNavigationBarButtonTap(e) {
			if (e.index === 0) {
				// 断开连接
				this.handleDisconnect()
			} else if (e.index === 1) {
				// 暂停/恢复截图
				this.toggleCapture()
			}
		},
		methods: {
			// 切换检测模型
			switchModel(key) {
				if (setModel(key)) {
					this.currentModel = key
					this.showModelPicker = false
				}
			},
			// ==================== 连接管理 ====================
			handleDisconnect() {
				this.stopScreenshotCapture()
				this.showFloatingImage = false
				uni.navigateBack({
					fail: () => {
						uni.redirectTo({ url: '/pages/index/index' })
					}
				})
			},

			onWebviewError(e) {
				log.error('WebView 加载失败:', e)
				this.stopScreenshotCapture()
				this.showFloatingImage = false
				uni.showModal({
					title: '连接失败',
					content: '无法连接到 ' + this.deviceIp + '\n\n请确保：\n1. 设备已开启\n2. Wi-Fi 已连接到设备热点\n3. 设备 IP 正确',
					showCancel: false,
					confirmText: '返回',
					success: (res) => {
						if (res.confirm) {
							uni.navigateBack({
								fail: () => { uni.redirectTo({ url: '/pages/index/index' }) }
							})
						}
					}
				})
			},

			// ==================== 截图控制 ====================
			startScreenshotCapture() {
				if (this.screenshotTimer) return
				this.isCapturing = true
				this._wasCapturing = false

				// 立即执行一次截图
				this.captureScreenshot()

				// 每 5 秒截图一次
				this.screenshotTimer = setInterval(() => {
					this.captureScreenshot()
				}, 5000)

				this._updateStatusBtn('\u25cf \u622a\u56fe ' + this.screenshotCount, 'rgba(37,99,235,0.88)')
			},

			pauseScreenshotCapture() {
				if (this.screenshotTimer) {
					clearInterval(this.screenshotTimer)
					this.screenshotTimer = null
				}
				this.isCapturing = false
				this._updateStatusBtn('\u23f8 \u5df2\u668f\u505c', 'rgba(234,179,8,0.88)')
			},

			stopScreenshotCapture() {
				this.pauseScreenshotCapture()
				this.screenshotCount = 0
				this._wasCapturing = false
			},

			toggleScreenshotCapture() {
				if (this.isCapturing) {
					this.pauseScreenshotCapture()
				} else {
					this.startScreenshotCapture()
				}
			},

			_updateStatusBtn(text, bg) {
				// #ifdef APP-PLUS
				try {
					const pages = getCurrentPages()
					const page = pages[pages.length - 1]
					const wv = page.$getAppWebview()
					if (wv) {
						wv.setStyle({
							titleNView: {
								type: 'transparent',
								backgroundColor: '#00000000',
								buttons: [
									NAV_BTN_DISCONNECT,
									{ ...NAV_BTN_SCREENSHOT, text, background: bg }
								]
							}
						})
					}
				} catch (e) {
					log.error('更新导航栏按钮失败:', e)
				}
				// #endif
			},

			// ==================== 截图核心逻辑 ====================
			captureScreenshot() {
				// #ifdef APP-PLUS
				try {
					// 获取当前页面 WebView
					const pages = getCurrentPages()
					const page = pages[pages.length - 1]
					const currentWebview = page.$getAppWebview()

					// 获取子 WebView（web-view 组件）
					const children = currentWebview.children()
					if (!children || children.length === 0) {
						log.warn('截图跳过：子 WebView 未就绪')
						return
					}
					const childWebview = children[0]

					// 创建 Bitmap 对象
					const bitmapId = 'ss_' + Date.now()
					const bitmap = new plus.nativeObj.Bitmap(bitmapId)

					// 将子 WebView 内容绘制到 Bitmap（指定区域截取）
					childWebview.draw(
						bitmap,
						() => {
							// 绘制成功，保存到本地
							this._saveBitmap(bitmap)
						},
						(e) => {
							log.error('截图绘制失败:', e)
							bitmap.clear()
						},
						{
							bit: 'ARGB',
							check: true,
							clip: SCREENSHOT_CLIP
						}
					)
				} catch (e) {
					log.error('截图异常:', e)
				}
				// #endif
			},

			_saveBitmap(bitmap) {
				// 生成时间戳文件名
				const ts = formatTimestamp(new Date())
				const filename = 'SS_' + ts + '.png'
				const savePath = '_doc/Screenshots/' + filename

				bitmap.save(
					savePath,
					{ format: 'png', quality: 100, overwrite: true },
					(res) => {
						this.screenshotCount++
						this.lastScreenshotPath = res.target
						this._updateStatusBtn(
							'\u25cf \u622a\u56fe ' + this.screenshotCount,
							'rgba(37,99,235,0.88)'
						)
						log.debug('截图已保存:', res.target)

						// 自动触发阿里云检测
						if (this.enableAutoDetect && !this.isDetecting) {
							// 检测完成后统一显示浮层，避免闪烁
							this.detectAndAnnotate(res.target, bitmap)
						} else {
							// 不检测时直接显示原始截图
							this.floatingImageSrc = res.target
							this.showFloatingImage = true
							bitmap.clear()
						}
					},
					(e) => {
						log.error('截图保存失败:', e)
						bitmap.clear()
					}
				)
			},

			// ==================== 阿里云检测与标注 ====================

			async detectAndAnnotate(imagePath, bitmap) {
				this.isDetecting = true
				this.lastError = ''
				log.debug('===== [检测] 开始新一轮检测 =====')
				log.debug('[检测] 图片路径:', imagePath)

				try {
					// 步骤1：上传图片到图床 → 调用阿里云检测
					log.debug('[检测] 步骤1: 上传图片并调用阿里云 API...')
					const response = await detectLocalImage(imagePath)
					log.debug('[检测] 步骤1: API 原始响应:', JSON.stringify(response).substring(0, 500))

					// 步骤2：解析检测结果
					log.debug('[检测] 步骤2: 解析检测结果...')
					const allObjects = parseDetectionResult(response)
					// 根据当前模型过滤（如"人物"模式只保留 human/person 等类别）
					const typeFilter = getCurrentFilter()
					const objects = typeFilter
						? allObjects.filter(obj => typeFilter.some(f => obj.name.toLowerCase().includes(f.toLowerCase())))
						: allObjects
					if (typeFilter) {
						log.debug(`[检测] 步骤2: 过滤后 ${objects.length}/${allObjects.length} 个物体 (模式: ${this.currentModelLabel})`)
					}
					this.detectionResults = objects
					log.debug(`[检测] 步骤2: 解析完成，检测到 ${objects.length} 个物体`)
					if (objects.length > 0) {
						objects.forEach((obj, i) => {
							log.debug(`[检测]   物体${i+1}: ${obj.name}, 置信度: ${(obj.score*100).toFixed(1)}%, 位置:`, obj.box)
						})
					}

					// 步骤3：绘制标注
				if (objects.length > 0) {
					log.debug('[检测] 步骤3: 开始绘制标注框...')
					// 传入 imagePath 作为回退：如果 Bitmap.toBase64Data() 失败，则从文件读取
					const annotatedPath = await this._drawAnnotations(bitmap, objects, imagePath)
					log.debug('[检测] 步骤3: 标注图片已生成:', annotatedPath)
					this.annotatedImageSrc = annotatedPath
					this.floatingImageSrc = annotatedPath
					this.showResultPopup = true
					this.showAlertPopup = true
					log.debug('[浮层] 已更新为标注图片')
				} else {
					log.debug('[检测] 未检测到物体，显示原始截图')
					this.floatingImageSrc = imagePath
					this.lastError = '未检测到物体'
					this.showAlertPopup = false
				}
				this.showFloatingImage = true

				} catch (error) {
					log.error('[检测] 管线异常:', error)
					log.error('[检测] 错误详情:', error.message, error.stack)
					this.lastError = '检测失败: ' + (error.message || '未知错误')
					// 检测失败仍显示原始截图
					this.floatingImageSrc = imagePath
					this.showFloatingImage = true
				} finally {
					this.isDetecting = false
					log.debug('===== [检测] 本轮结束 =====')
					try { bitmap.clear() } catch (ex) {}
				}
			},

			/**
			 * 在图片上绘制检测结果的标注框
			 * 使用 Android 原生 Canvas API 绘制，不依赖 document 或新版 Bitmap API
			 */
			_drawAnnotations(bitmap, objects, imagePath) {
				return new Promise((resolve, reject) => {
					// #ifdef APP-PLUS
					try {
						log.debug('[标注] 使用 Android Native Canvas 绘制...')
						log.debug(`[标注] 检测到 ${objects.length} 个物体，图片: ${imagePath}`)

						// 复用缓存的 Android 原生类：首次调用执行 importClass，后续直接读取
						if (!nativeClassCache) {
							nativeClassCache = {
								BitmapFactory: plus.android.importClass('android.graphics.BitmapFactory'),
								Bitmap: plus.android.importClass('android.graphics.Bitmap'),
								Canvas: plus.android.importClass('android.graphics.Canvas'),
								Paint: plus.android.importClass('android.graphics.Paint'),
								Color: plus.android.importClass('android.graphics.Color'),
								FileOutputStream: plus.android.importClass('java.io.FileOutputStream'),
								File: plus.android.importClass('java.io.File'),
								BitmapCompressFormat: plus.android.importClass('android.graphics.Bitmap$CompressFormat'),
								PaintStyle: plus.android.importClass('android.graphics.Paint$Style')
							}
						}
						const { BitmapFactory, Bitmap, Canvas, Paint, Color, FileOutputStream, File, BitmapCompressFormat, PaintStyle } = nativeClassCache

						// 转换 file:// 路径为 Android 原生路径
						const nativePath = imagePath.replace('file://', '')
						log.debug('[标注] 原生路径:', nativePath)

						// 加载图片
						const srcBitmap = BitmapFactory.decodeFile(nativePath)
						if (!srcBitmap) {
							reject(new Error('无法解码图片: ' + nativePath))
							return
						}

						const imgWidth = srcBitmap.getWidth()
						const imgHeight = srcBitmap.getHeight()
						log.debug(`[标注] 图片尺寸: ${imgWidth}x${imgHeight}`)

						// 创建可绘制的 Bitmap 副本
						const drawBitmap = srcBitmap.copy(Bitmap.Config.ARGB_8888, true)
						srcBitmap.recycle()

						const canvas = new Canvas(drawBitmap)

						// ---- 检测框画笔 ----
						const rectPaint = new Paint()
						rectPaint.setColor(Color.RED)
						rectPaint.setStyle(PaintStyle.STROKE)
						rectPaint.setStrokeWidth(6)
						rectPaint.setAntiAlias(true)

						// ---- 标签背景画笔 ----
						const labelBgPaint = new Paint()
						labelBgPaint.setColor(Color.argb(179, 255, 0, 0)) // rgba(255,0,0,0.7)
						labelBgPaint.setStyle(PaintStyle.FILL)

						// ---- 标签文字画笔 ----
						const textPaint = new Paint()
						textPaint.setColor(Color.WHITE)
						textPaint.setTextSize(36)
						textPaint.setAntiAlias(true)
						textPaint.setFakeBoldText(true)

						objects.forEach((obj, i) => {
							const box = obj.box
							if (!box) return

							const left = Math.max(0, box.left || 0)
							const top = Math.max(0, box.top || 0)
							const w = Math.min(box.width || 100, imgWidth - left)
							const h = Math.min(box.height || 100, imgHeight - top)

							// 绘制红色检测框
							canvas.drawRect(left, top, left + w, top + h, rectPaint)

							// 绘制标签
							const labelText = `${obj.name} ${(obj.score * 100).toFixed(0)}%`
							const labelH = 52
							const labelW = textPaint.measureText(labelText) + 32
							const labelY = Math.max(0, top - labelH - 8)

							canvas.drawRect(left, labelY, left + labelW, labelY + labelH, labelBgPaint)
							canvas.drawText(labelText, left + 16, labelY + 36, textPaint)

							log.debug(`[标注]   框${i+1}: ${labelText} @ (${left},${top}) ${w}x${h}`)
						})

						// 保存标注图片
						const ts = formatTimestamp(new Date())

						// 与原始截图同目录
						const dir = imagePath.substring(0, imagePath.lastIndexOf('/'))
						const annotatedPath = dir + '/SS_Annotated_' + ts + '.png'
						const nativeOutPath = annotatedPath.replace('file://', '')

						log.debug('[标注] 保存到:', nativeOutPath)
						const outFile = new File(nativeOutPath)
						const fos = new FileOutputStream(outFile)
						drawBitmap.compress(BitmapCompressFormat.PNG, 100, fos)
						fos.close()
						drawBitmap.recycle()

						log.debug('[标注] 标注图片已保存:', annotatedPath)
						resolve(annotatedPath)

					} catch (e) {
						log.error('[标注] 绘制异常:', e.message, e.stack)
						reject(e)
					}
					// #endif
				})
			},

			}
	}
</script>

<style>
	.webview-container {
		width: 100vw;
		height: 100vh;
		overflow: hidden;
	}

	/* 模型选择器 */
	.model-selector {
		position: fixed;
		bottom: 540rpx;
		left: 1740rpx;
		z-index: 1000;
	}

	.model-btn {
		padding: 20rpx 20rpx;
		background: rgba(0, 0, 0, 0.65);
		border-radius: 24rpx;
	}

	.model-current {
		font-size: 22rpx;
		color: #ffffff;
	}

	.model-dropdown {
		position: absolute;
		bottom: 56rpx;
		left: 0;
		background: rgba(0, 0, 0, 0.85);
		border-radius: 16rpx;
		overflow: hidden;
		min-width: 180rpx;
	}

	.model-option {
		display: flex;
		align-items: center;
		justify-content: space-between;
		padding: 14rpx 24rpx;
		font-size: 22rpx;
		color: rgba(255, 255, 255, 0.8);
		border-bottom: 1rpx solid rgba(255, 255, 255, 0.1);
	}

	.model-option:last-child {
		border-bottom: none;
	}

	.model-option-active {
		color: #60a5fa;
		background: rgba(96, 165, 250, 0.15);
	}

	.model-check {
		color: #60a5fa;
		font-size: 22rpx;
		margin-left: 16rpx;
	}

	/* AI 检测状态提示 */
	.detect-toast {
		position: fixed;
		top: 100rpx;
		right: 24rpx;
		z-index: 999;
		display: flex;
		align-items: center;
		padding: 12rpx 24rpx;
		background: rgba(59, 130, 246, 0.95);
		border-radius: 32rpx;
		box-shadow: 0 4rpx 20rpx rgba(0, 0, 0, 0.3);
	}

	.detect-loading {
		width: 28rpx;
		height: 28rpx;
		margin-right: 12rpx;
		border: 3rpx solid rgba(255, 255, 255, 0.3);
		border-top-color: #ffffff;
		border-radius: 50%;
		animation: spin 0.8s linear infinite;
	}

	@keyframes spin {
		to { transform: rotate(360deg); }
	}

	.detect-text {
		font-size: 22rpx;
		color: #ffffff;
		font-weight: 500;
	}

	/* 检测结果弹窗 */
	.result-popup {
		position: fixed;
		top: 0;
		left: 0;
		right: 0;
		bottom: 0;
		background: rgba(0, 0, 0, 0.6);
		z-index: 1000;
		display: flex;
		align-items: center;
		justify-content: center;
	}

	.result-content {
		width: 620rpx;
		max-height: 80vh;
		background: #1a1a2e;
		border-radius: 24rpx;
		overflow: hidden;
		box-shadow: 0 8rpx 40rpx rgba(0, 0, 0, 0.5);
	}

	/* 标注图片预览 */
	.result-image-wrap {
		width: 100%;
		max-height: 400rpx;
		overflow: hidden;
		background: #000;
	}

	.result-image {
		width: 100%;
		display: block;
	}

	.result-header {
		display: flex;
		align-items: center;
		justify-content: space-between;
		padding: 28rpx 32rpx;
		border-bottom: 1rpx solid rgba(255, 255, 255, 0.1);
	}

	.result-title {
		font-size: 30rpx;
		font-weight: 600;
		color: #ffffff;
	}

	.result-close {
		font-size: 40rpx;
		color: #94a3b8;
		line-height: 1;
	}

	.result-list {
		max-height: 400rpx;
		padding: 16rpx 0;
	}

	.result-item {
		display: flex;
		align-items: center;
		justify-content: space-between;
		padding: 20rpx 32rpx;
		border-bottom: 1rpx solid rgba(255, 255, 255, 0.05);
	}

	.item-name {
		font-size: 26rpx;
		color: #e2e8f0;
	}

	.item-score {
		font-size: 24rpx;
		color: #3b82f6;
		font-weight: 600;
	}

	.no-result {
		padding: 60rpx 32rpx;
		text-align: center;
	}

	.no-result text {
		font-size: 26rpx;
		color: #64748b;
	}

	/* 物体检测警告弹窗 */
	.alert-popup {
		position: fixed;
		top: 1100rpx;
		left: 1300rpx;
		right: 800rpx;
		z-index: 998;
		background: linear-gradient(135deg, #7f1d1d 0%, #dc2626 100%);
		border: 2rpx solid rgba(255, 255, 255, 0.3);
		border-radius: 20rpx;
		box-shadow: 0 8rpx 32rpx rgba(220, 38, 38, 0.35);
		overflow: hidden;
		animation: alertSlideIn 0.3s ease-out;
	}

	@keyframes alertSlideIn {
		from {
			opacity: 0;
			transform: translateY(-30rpx);
		}
		to {
			opacity: 1;
			transform: translateY(0);
		}
	}

	.alert-popup-header {
		display: flex;
		align-items: center;
		padding: 20rpx 24rpx;
		background: rgba(234, 179, 8, 0.15);
		border-bottom: 1rpx solid rgba(234, 179, 8, 0.2);
	}

	.alert-popup-icon {
		font-size: 32rpx;
		margin-right: 12rpx;
		color: #facc15;
	}

	.alert-popup-title {
		flex: 1;
		font-size: 26rpx;
		font-weight: 600;
		color: #facc15;
	}

	.alert-popup-close {
		font-size: 36rpx;
		color: #94a3b8;
		line-height: 1;
		padding: 0 8rpx;
	}

	.alert-popup-body {
		padding: 12rpx 24rpx 20rpx;
		max-height: 300rpx;
		overflow-y: auto;
	}

	.alert-item {
		display: flex;
		align-items: center;
		justify-content: space-between;
		padding: 14rpx 0;
		border-bottom: 1rpx solid rgba(255, 255, 255, 0.06);
	}

	.alert-item:last-child {
		border-bottom: none;
	}

	.alert-item-name {
		font-size: 24rpx;
		color: #e2e8f0;
		font-weight: 500;
	}

	.alert-item-score {
		font-size: 22rpx;
		color: #facc15;
		font-weight: 600;
		background: rgba(234, 179, 8, 0.12);
		padding: 4rpx 16rpx;
		border-radius: 16rpx;
	}

	/* 右下角浮层：实时标注图片（cover-view） */
	.floating-annotated {
		position: fixed;
		right: 16rpx;
		bottom: 16rpx;
		width: 640rpx;
		height: 520rpx;
		background-color: rgba(0, 0, 0, 0.85);
		border-radius: 8rpx;
	}

	.floating-close {
		position: absolute;
		top: 0;
		right: 0;
		width: 50rpx;
		height: 50rpx;
		padding: 4rpx;
		text-align: center;
		font-size: 36rpx;
		line-height: 46rpx;
		color: #ffffff;
		background-color: rgba(0, 0, 0, 0.6);
	}

	.floating-image {
		width: 100%;
		height: 100%;
	}

	.floating-label {
		position: absolute;
		bottom: 0;
		left: 0;
		right: 0;
		padding: 8rpx 16rpx;
		background-color: rgba(239, 68, 68, 0.85);
		color: #ffffff;
		font-size: 20rpx;
		text-align: center;
	}
</style>
