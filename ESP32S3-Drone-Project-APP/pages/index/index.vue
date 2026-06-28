<template>
	<view class="main-container">
		<!-- 连接按钮区域 -->
		<view class="connect-area">
			<view 
				class="connect-btn" 
				:class="{ 'btn-loading': isConnecting, 'btn-active': isPressed }"
				@touchstart="onPressStart"
				@touchend="onPressEnd"
				@touchcancel="onPressEnd"
				@tap="handleConnect"
			>
				<view class="btn-spinner" v-if="isConnecting"></view>
				<text class="btn-text">{{ isConnecting ? '连接中...' : '连接' }}</text>
			</view>
		</view>

		<!-- 中间提示区域 -->
		<view class="info-area">
			<text class="info-title">ESP32 无人机上位机</text>
			<text class="info-subtitle">输入设备IP地址后点击连接</text>

			<!-- IP 输入框 -->
			<view class="ip-input-wrapper">
				<input
					class="ip-input"
					type="text"
					v-model="deviceIp"
					placeholder="请输入IP地址"
					placeholder-class="ip-placeholder"
				/>
			</view>

			<!-- 截图文件入口 -->
			<view class="recordings-entry" @tap="openRecordings">
				<text class="entry-icon">&#x1F5BC;</text>
				<text class="entry-text">截图文件</text>
				<text class="entry-arrow">&#x203A;</text>
			</view>
		</view>

		<!-- 错误提示 -->
		<view class="error-toast" v-if="errorMsg" @tap="errorMsg = ''">
			<text class="error-text">{{ errorMsg }}</text>
		</view>
	</view>
</template>

<script>
	export default {
		data() {
			return {
				deviceIp: '192.168.4.1',
				isConnecting: false,
				isPressed: false,
				errorMsg: '',
				connectTimer: null
			}
		},
		onShow() {
			// 从 WebView 页面返回时重置状态，确保可再次连接
			this.isConnecting = false
			this.isPressed = false
		},
		methods: {
			onPressStart() {
				if (!this.isConnecting) {
					this.isPressed = true
				}
			},
			onPressEnd() {
				this.isPressed = false
			},
			handleConnect() {
				if (this.isConnecting) return

				// 验证 IP 地址
				const ip = this.deviceIp.trim()
				if (!ip) {
					this.errorMsg = '请输入IP地址'
					return
				}

				// 简单的 IP 格式验证
				const ipPattern = /^(\d{1,3}\.){3}\d{1,3}$/
				if (!ipPattern.test(ip)) {
					this.errorMsg = 'IP地址格式不正确'
					return
				}

				this.isConnecting = true
				this.errorMsg = ''

				this.connectTimer = setTimeout(() => {
					uni.navigateTo({
						url: '/pages/webview/webview?ip=' + encodeURIComponent(ip),
						fail: (err) => {
							console.error('跳转失败:', err)
							this.isConnecting = false
							this.errorMsg = '页面跳转失败，请重试'
						}
					})
				}, 300)
			},
			openRecordings() {
				uni.navigateTo({
					url: '/pages/recordings/recordings'
				})
			}
		},
		onUnload() {
			if (this.connectTimer) {
				clearTimeout(this.connectTimer)
				this.connectTimer = null
			}
		}
	}
</script>

<style scoped>
	.main-container {
		width: 100vw;
		height: 100vh;
		background: linear-gradient(135deg, #0f0f23 0%, #1a1a3e 50%, #0d1b3e 100%);
		display: flex;
		flex-direction: column;
		position: relative;
		overflow: hidden;
	}

	/* 连接按钮 */
	.connect-area {
		position: absolute;
		top: 24rpx;
		left: 24rpx;
		z-index: 100;
	}

	.connect-btn {
		display: flex;
		align-items: center;
		justify-content: center;
		padding: 14rpx 28rpx;
		background: linear-gradient(135deg, #2563eb 0%, #1d4ed8 100%);
		border-radius: 36rpx;
		box-shadow: 0 4rpx 16rpx rgba(37, 99, 235, 0.35);
		transition: all 0.15s ease;
		min-width: 120rpx;
	}

	.connect-btn.btn-active {
		transform: scale(0.95);
		box-shadow: 0 2rpx 8rpx rgba(37, 99, 235, 0.25);
		background: linear-gradient(135deg, #1d4ed8 0%, #1e40af 100%);
	}

	.connect-btn.btn-loading {
		background: linear-gradient(135deg, #1e40af 0%, #1e3a8a 100%);
		opacity: 0.85;
	}

	.btn-spinner {
		width: 24rpx;
		height: 24rpx;
		margin-right: 10rpx;
		border: 3rpx solid rgba(255, 255, 255, 0.3);
		border-top-color: #ffffff;
		border-radius: 50%;
		animation: spin 0.7s linear infinite;
	}

	@keyframes spin {
		to { transform: rotate(360deg); }
	}

	.btn-text {
		font-size: 26rpx;
		font-weight: 600;
		color: #ffffff;
	}

	/* 中间信息 */
	.info-area {
		flex: 1;
		display: flex;
		flex-direction: column;
		align-items: center;
		justify-content: center;
		padding: 40rpx;
	}

	.info-title {
		font-size: 32rpx;
		font-weight: 700;
		color: #e2e8f0;
		margin-bottom: 16rpx;
		letter-spacing: 2rpx;
	}

	.info-subtitle {
		font-size: 22rpx;
		color: #64748b;
		margin-bottom: 20rpx;
	}

	/* IP 输入框 */
	.ip-input-wrapper {
		width: 400rpx;
		margin-bottom: 20rpx;
	}

	.ip-input {
		width: 100%;
		height: 72rpx;
		background: rgba(255, 255, 255, 0.08);
		border: 1rpx solid rgba(37, 99, 235, 0.3);
		border-radius: 16rpx;
		padding: 0 24rpx;
		font-size: 28rpx;
		color: #e2e8f0;
		text-align: center;
	}

	.ip-placeholder {
		color: #64748b;
	}

	/* 录制文件入口 */
	.recordings-entry {
		margin-top: 28rpx;
		display: flex;
		align-items: center;
		padding: 18rpx 28rpx;
		background: rgba(255, 255, 255, 0.06);
		border: 1rpx solid rgba(255, 255, 255, 0.1);
		border-radius: 20rpx;
	}

	.recordings-entry:active {
		background: rgba(37, 99, 235, 0.2);
	}

	.entry-icon {
		font-size: 32rpx;
		margin-right: 14rpx;
	}

	.entry-text {
		font-size: 26rpx;
		color: #cbd5e1;
		font-weight: 500;
		flex: 1;
	}

	.entry-arrow {
		font-size: 32rpx;
		color: #64748b;
	}

	/* 错误提示 */
	.error-toast {
		position: absolute;
		bottom: 40rpx;
		left: 50%;
		transform: translateX(-50%);
		background: rgba(239, 68, 68, 0.9);
		padding: 16rpx 32rpx;
		border-radius: 24rpx;
		box-shadow: 0 4rpx 16rpx rgba(239, 68, 68, 0.3);
		z-index: 200;
	}

	.error-text {
		font-size: 22rpx;
		color: #ffffff;
	}
</style>