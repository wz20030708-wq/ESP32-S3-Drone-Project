<template>
	<view class="page-container">
		<!-- 标题栏 -->
		<view class="header">
			<text class="back-btn" @tap="goBack">&#x2190; 返回</text>
			<text class="header-title">截图文件</text>
			<text class="header-count" v-if="fileList.length">{{ fileList.length }} 张图片</text>
			<text class="select-btn" @tap="toggleSelectionMode" v-if="fileList.length">
				{{ selectionMode ? '取消' : '选择' }}
			</text>
		</view>

		<!-- 文件列表 -->
		<scroll-view 
			class="file-list" 
			scroll-y 
			:style="{ height: scrollHeight + 'px' }"
			v-if="fileList.length > 0"
		>
			<view 
				class="file-item" 
				:class="{ 'file-item-selected': selectionMode && isSelected(index) }"
				v-for="(file, index) in fileList" 
				:key="index"
				@tap="onFileTap(file, index)"
				@longpress="onFileLongPress(file, index)"
			>
				<!-- 选择模式下的复选框 -->
				<view class="file-checkbox" v-if="selectionMode" @tap.stop="toggleSelect(index)">
					<view class="checkbox-box" :class="{ 'checkbox-checked': isSelected(index) }">
						<text class="checkbox-mark" v-if="isSelected(index)">&#x2713;</text>
					</view>
				</view>
				<view class="file-icon">&#x1F5BC;</view>
				<view class="file-info">
					<text class="file-name">{{ file.name }}</text>
					<text class="file-date">{{ file.date }}</text>
					<text class="file-size">{{ file.size }}</text>
				</view>
				<view class="file-preview" v-if="!selectionMode">&#x1F50D;</view>
			</view>
			<!-- 底部留白 -->
			<view style="height: 100rpx;"></view>
		</scroll-view>

		<!-- 空状态 -->
		<view class="empty-state" v-if="fileList.length === 0">
			<text class="empty-icon">&#x1F5BC;</text>
			<text class="empty-text">暂无截图文件</text>
			<text class="empty-hint">连接设备后将自动开始截图</text>
		</view>

		<!-- 底部操作栏（选择模式） -->
		<view class="action-bar" v-if="selectionMode && fileList.length > 0">
			<text class="action-btn" @tap="selectAll">全选</text>
			<text class="action-btn action-delete" @tap="batchDelete">
				删除({{ selectedCount }})
			</text>
			<text class="action-btn action-save" @tap="batchSaveToAlbum">
				保存到相册({{ selectedCount }})
			</text>
		</view>

		<!-- 底部提示（非选择模式） -->
		<view class="footer-hint" v-if="!selectionMode && fileList.length > 0">
			<text>点击预览 · 长按进入多选</text>
		</view>

		<!-- ==================== 图片预览弹窗 ==================== -->
		<view class="preview-overlay" v-if="previewVisible" @tap="closePreview">
			<view class="preview-topbar">
				<text class="preview-close" @tap="closePreview">&#x2715;</text>
				<text class="preview-counter">{{ previewIndex + 1 }} / {{ fileList.length }}</text>
				<text class="preview-filename">{{ fileList[previewIndex] && fileList[previewIndex].name }}</text>
			</view>
			<view class="preview-body" @tap.stop>
				<!-- 左箭头（未缩放时显示） -->
				<view 
					class="preview-arrow preview-arrow-left" 
					@tap.stop="previewPrev"
					v-if="previewIndex > 0 && imageScale <= 1"
				>
					<text>&#x2039;</text>
				</view>
				<!-- 图片容器（支持缩放与拖拽） -->
				<movable-area 
					class="preview-movable" 
					:scale-area="true"
					:key="'area-' + (previewKey || 0)"
				>
					<movable-view 
						class="preview-movable-view"
						direction="all"
						:scale="true"
						:scale-min="1"
						:scale-max="4"
						@scale="onScaleChange"
						@end="onMoveEnd"
					>
						<image 
							:src="fileList[previewIndex] && fileList[previewIndex].path"
							mode="aspectFit"
							class="preview-image"
						></image>
					</movable-view>
				</movable-area>
				<!-- 缩放提示 -->
				<view class="preview-zoom-hint" v-if="imageScale > 1">
					<text>双指缩放中 ×{{ imageScale.toFixed(1) }}</text>
					<text class="preview-zoom-reset" @tap.stop="resetZoom">重置</text>
				</view>
				<!-- 右箭头（未缩放时显示） -->
				<view 
					class="preview-arrow preview-arrow-right" 
					@tap.stop="previewNext"
					v-if="previewIndex < fileList.length - 1 && imageScale <= 1"
				>
					<text>&#x203A;</text>
				</view>
			</view>
			<view class="preview-bottombar">
				<text class="preview-action" @tap="previewSave">保存到相册</text>
				<text class="preview-action preview-action-delete" @tap="previewDelete">删除</text>
			</view>
		</view>
	</view>
</template>

<script>
	export default {
		data() {
			return {
				fileList: [],
				selectionMode: false,
				selectedIndices: [],
				scrollHeight: 400,
				// 预览相关
				previewVisible: false,
				previewIndex: 0,
				imageScale: 1,
				previewKey: 0
			}
		},
		computed: {
			selectedCount() {
				return this.selectedIndices.length
			}
		},
		onReady() {
			// 计算 scroll-view 可用高度
			const sysInfo = uni.getSystemInfoSync()
			// 在横屏模式下，windowHeight 是短边，减去 header(~80px) 和底部栏(~60px)
			this.scrollHeight = sysInfo.windowHeight - 80
		},
		onShow() {
			this.loadFiles()
		},
		onUnload() {
			this.selectionMode = false
			this.selectedIndices = []
		},
		methods: {
			goBack() {
				uni.navigateBack()
			},

			// ==================== 文件加载 ====================
			loadFiles() {
				// #ifdef APP-PLUS
				try {
					// 将 _doc 相对路径转为绝对路径（兼容不同版本）
					let absPath = plus.io.convertLocalFileSystemURL('_doc/Screenshots/')
					console.log('截图目录路径:', absPath)

					const dir = plus.android.newObject('java.io.File', absPath)

					if (!plus.android.invoke(dir, 'exists')) {
						plus.android.invoke(dir, 'mkdirs')
						console.log('目录不存在，已创建')
						this.fileList = []
						return
					}

					const files = plus.android.invoke(dir, 'listFiles')
					if (!files || files.length === 0) {
						console.log('目录为空')
						this.fileList = []
						return
					}

					const result = []
					const len = files.length
					const sdf = plus.android.newObject('java.text.SimpleDateFormat', 'yyyy-MM-dd HH:mm:ss')

					for (let i = 0; i < len; i++) {
						const f = files[i]
						const name = plus.android.invoke(f, 'getName')
						if (!name || !name.endsWith('.png')) continue

						const path = plus.android.invoke(f, 'getAbsolutePath')
						const lastModified = plus.android.invoke(f, 'lastModified')
						const sizeBytes = plus.android.invoke(f, 'length')

						const date = plus.android.invoke(sdf, 'format',
							plus.android.newObject('java.util.Date', lastModified))

						let size = ''
						if (sizeBytes < 1024) {
							size = sizeBytes + ' B'
						} else if (sizeBytes < 1024 * 1024) {
							size = Math.round(sizeBytes / 1024) + ' KB'
						} else {
							size = (sizeBytes / (1024 * 1024)).toFixed(1) + ' MB'
						}

						result.push({
							name: name,
							path: path,
							date: date,
							size: size
						})
					}

					result.sort((a, b) => b.date.localeCompare(a.date))
					this.fileList = result
					console.log('加载截图文件:', result.length, '张')
				} catch (e) {
					console.error('读取截图文件失败:', e)
					this.fileList = []
				}
				// #endif
			},

			// ==================== 文件操作 ====================
			onFileTap(file, index) {
				if (this.selectionMode) {
					this.toggleSelect(index)
				} else {
					this.previewFile(file, index)
				}
			},

			onFileLongPress(file, index) {
				if (!this.selectionMode) {
					this.selectionMode = true
					this.selectedIndices = [index]
				}
			},

			previewFile(file, index) {
				this.previewIndex = index
				this.imageScale = 1
				this.previewVisible = true
			},

			closePreview() {
				this.previewVisible = false
				this.imageScale = 1
			},

			previewPrev() {
				if (this.previewIndex > 0) {
					this.previewIndex--
					this.imageScale = 1
				}
			},

			previewNext() {
				if (this.previewIndex < this.fileList.length - 1) {
					this.previewIndex++
					this.imageScale = 1
				}
			},

			onScaleChange(e) {
				this.imageScale = e.detail.scale || 1
			},

			onMoveEnd(e) {
				// movable-view 移动结束后同步 scale
				if (e.detail && e.detail.scale !== undefined) {
					this.imageScale = e.detail.scale
				}
			},

			resetZoom() {
				this.imageScale = 1
				// 强制重置 movable-view：通过切换 key 触发重新渲染
				this.previewKey = (this.previewKey || 0) + 1
			},

			previewSave() {
				const file = this.fileList[this.previewIndex]
				if (file) {
					this.saveSingleToAlbum(file)
				}
			},

			previewDelete() {
				const file = this.fileList[this.previewIndex]
				if (!file) return
				uni.showModal({
					title: '删除确认',
					content: '确定删除 ' + file.name + ' ？',
					cancelText: '取消',
					confirmText: '删除',
					confirmColor: '#ef4444',
					success: (res) => {
						if (res.confirm) {
							// #ifdef APP-PLUS
							try {
								const f = plus.android.newObject('java.io.File', file.path)
								if (plus.android.invoke(f, 'exists')) {
									plus.android.invoke(f, 'delete')
								}
							} catch (e) {
								console.error('删除失败:', e)
							}
							// #endif
							// 如果删的是最后一张，索引前移
							if (this.previewIndex >= this.fileList.length - 1 && this.previewIndex > 0) {
								this.previewIndex--
							}
							// 如果删完没了，关闭预览
							if (this.fileList.length <= 1) {
								this.previewVisible = false
							}
							this.loadFiles()
							uni.showToast({ title: '已删除', icon: 'none' })
						}
					}
				})
			},

			// ==================== 选择模式 ====================
			toggleSelectionMode() {
				this.selectionMode = !this.selectionMode
				if (!this.selectionMode) {
					this.selectedIndices = []
				}
			},

			isSelected(index) {
				return this.selectedIndices.indexOf(index) !== -1
			},

			toggleSelect(index) {
				const pos = this.selectedIndices.indexOf(index)
				if (pos === -1) {
					this.selectedIndices.push(index)
				} else {
					this.selectedIndices.splice(pos, 1)
				}
			},

			selectAll() {
				if (this.selectedIndices.length === this.fileList.length) {
					this.selectedIndices = []
				} else {
					this.selectedIndices = this.fileList.map((_, i) => i)
				}
			},

			// ==================== 批量删除 ====================
			batchDelete() {
				if (this.selectedIndices.length === 0) {
					uni.showToast({ title: '请选择要删除的图片', icon: 'none' })
					return
				}

				uni.showModal({
					title: '删除确认',
					content: '确定删除选中的 ' + this.selectedIndices.length + ' 张截图？',
					cancelText: '取消',
					confirmText: '删除',
					confirmColor: '#ef4444',
					success: (res) => {
						if (res.confirm) {
							this._executeBatchDelete()
						}
					}
				})
			},

			_executeBatchDelete() {
				// #ifdef APP-PLUS
				try {
					const sorted = [...this.selectedIndices].sort((a, b) => b - a)
					let deletedCount = 0
					for (const index of sorted) {
						const file = this.fileList[index]
						if (file && file.path) {
							const f = plus.android.newObject('java.io.File', file.path)
							if (plus.android.invoke(f, 'exists')) {
								const deleted = plus.android.invoke(f, 'delete')
								if (deleted) deletedCount++
							}
						}
					}
					this.selectedIndices = []
					this.selectionMode = false
					this.loadFiles()
					uni.showToast({ title: '已删除 ' + deletedCount + ' 张', icon: 'none' })
				} catch (e) {
					console.error('批量删除失败:', e)
					uni.showToast({ title: '删除失败', icon: 'none' })
				}
				// #endif
			},

			// ==================== 批量保存到相册 ====================
			batchSaveToAlbum() {
				if (this.selectedIndices.length === 0) {
					uni.showToast({ title: '请选择要保存的图片', icon: 'none' })
					return
				}

				uni.showLoading({ title: '保存中...', mask: true })
				this._saveSelectedToAlbum(0)
			},

			_saveSelectedToAlbum(currentIndex) {
				if (currentIndex >= this.selectedIndices.length) {
					uni.hideLoading()
					this.selectedIndices = []
					this.selectionMode = false
					uni.showToast({ title: '保存完成', icon: 'none' })
					return
				}

				const file = this.fileList[this.selectedIndices[currentIndex]]
				if (!file || !file.path) {
					this._saveSelectedToAlbum(currentIndex + 1)
					return
				}

				// #ifdef APP-PLUS
				plus.gallery.save(
					file.path,
					() => {
						this._saveSelectedToAlbum(currentIndex + 1)
					},
					(err) => {
						console.error('保存失败:', file.name, err)
						this._saveSelectedToAlbum(currentIndex + 1)
					}
				)
				// #endif
				// #ifndef APP-PLUS
				uni.saveImageToPhotosAlbum({
					filePath: file.path,
					success: () => {
						this._saveSelectedToAlbum(currentIndex + 1)
					},
					fail: (err) => {
						console.error('保存失败:', file.name, err)
						this._saveSelectedToAlbum(currentIndex + 1)
					}
				})
				// #endif
			},

			saveSingleToAlbum(file) {
				// #ifdef APP-PLUS
				plus.gallery.save(
					file.path,
					() => {
						uni.showToast({ title: '已保存到相册', icon: 'none' })
					},
					(err) => {
						console.error('保存失败:', err)
						uni.showToast({ title: '保存失败', icon: 'none' })
					}
				)
				// #endif
				// #ifndef APP-PLUS
				uni.saveImageToPhotosAlbum({
					filePath: file.path,
					success: () => {
						uni.showToast({ title: '已保存到相册', icon: 'none' })
					},
					fail: (err) => {
						console.error('保存失败:', err)
						uni.showToast({ title: '保存失败', icon: 'none' })
					}
				})
				// #endif
			}
		}
	}
</script>

<style scoped>
	.page-container {
		width: 100vw;
		height: 100vh;
		background: linear-gradient(135deg, #0f0f23 0%, #1a1a3e 50%, #0d1b3e 100%);
		display: flex;
		flex-direction: column;
	}

	/* 标题栏 */
	.header {
		display: flex;
		align-items: center;
		padding: 30rpx 30rpx 20rpx;
		flex-shrink: 0;
	}

	.back-btn {
		font-size: 26rpx;
		color: #60a5fa;
		padding: 10rpx 0;
	}

	.header-title {
		font-size: 32rpx;
		font-weight: 700;
		color: #e2e8f0;
		margin-left: 24rpx;
		flex: 1;
	}

	.header-count {
		font-size: 22rpx;
		color: #64748b;
		margin-right: 20rpx;
	}

	.select-btn {
		font-size: 24rpx;
		color: #60a5fa;
		padding: 10rpx 16rpx;
		border: 1rpx solid rgba(96, 165, 250, 0.3);
		border-radius: 12rpx;
	}

	.select-btn:active {
		background: rgba(96, 165, 250, 0.15);
	}

	/* 文件列表 */
	.file-list {
		padding: 0 30rpx;
		/* height 通过 :style 动态绑定 */
	}

	.file-item {
		display: flex;
		align-items: center;
		padding: 24rpx 20rpx;
		margin-bottom: 12rpx;
		background: rgba(255, 255, 255, 0.05);
		border-radius: 16rpx;
		border: 1rpx solid rgba(255, 255, 255, 0.06);
		transition: background 0.15s;
	}

	.file-item:active {
		background: rgba(37, 99, 235, 0.2);
	}

	.file-item-selected {
		background: rgba(37, 99, 235, 0.15);
		border-color: rgba(37, 99, 235, 0.3);
	}

	/* 复选框 */
	.file-checkbox {
		margin-right: 16rpx;
		flex-shrink: 0;
	}

	.checkbox-box {
		width: 40rpx;
		height: 40rpx;
		border: 2rpx solid rgba(255, 255, 255, 0.3);
		border-radius: 8rpx;
		display: flex;
		align-items: center;
		justify-content: center;
		transition: all 0.15s;
	}

	.checkbox-checked {
		background: #2563eb;
		border-color: #2563eb;
	}

	.checkbox-mark {
		font-size: 26rpx;
		color: #ffffff;
		font-weight: bold;
	}

	.file-icon {
		font-size: 40rpx;
		margin-right: 20rpx;
	}

	.file-info {
		flex: 1;
		display: flex;
		flex-direction: column;
	}

	.file-name {
		font-size: 26rpx;
		color: #e2e8f0;
		font-weight: 500;
		margin-bottom: 6rpx;
		word-break: break-all;
	}

	.file-date {
		font-size: 20rpx;
		color: #64748b;
		margin-bottom: 4rpx;
	}

	.file-size {
		font-size: 20rpx;
		color: #475569;
	}

	.file-preview {
		width: 52rpx;
		height: 52rpx;
		display: flex;
		align-items: center;
		justify-content: center;
		background: rgba(37, 99, 235, 0.3);
		border-radius: 50%;
		font-size: 24rpx;
		color: #60a5fa;
		flex-shrink: 0;
	}

	/* 空状态 */
	.empty-state {
		flex: 1;
		display: flex;
		flex-direction: column;
		align-items: center;
		justify-content: center;
	}

	.empty-icon {
		font-size: 80rpx;
		margin-bottom: 20rpx;
	}

	.empty-text {
		font-size: 28rpx;
		color: #94a3b8;
		margin-bottom: 10rpx;
	}

	.empty-hint {
		font-size: 20rpx;
		color: #475569;
	}

	/* 底部操作栏 */
	.action-bar {
		display: flex;
		align-items: center;
		justify-content: space-around;
		padding: 20rpx 30rpx;
		background: rgba(15, 15, 35, 0.95);
		border-top: 1rpx solid rgba(255, 255, 255, 0.08);
		flex-shrink: 0;
	}

	.action-btn {
		font-size: 24rpx;
		color: #cbd5e1;
		padding: 14rpx 28rpx;
		border-radius: 12rpx;
		background: rgba(255, 255, 255, 0.08);
	}

	.action-btn:active {
		background: rgba(255, 255, 255, 0.15);
	}

	.action-delete {
		color: #fca5a5;
	}

	.action-save {
		color: #60a5fa;
	}

	/* 底部提示 */
	.footer-hint {
		text-align: center;
		padding: 20rpx;
		font-size: 20rpx;
		color: #475569;
		flex-shrink: 0;
	}

	/* ==================== 图片预览弹窗 ==================== */
	.preview-overlay {
		position: fixed;
		top: 0;
		left: 0;
		right: 0;
		bottom: 0;
		z-index: 9999;
		background: rgba(0, 0, 0, 0.95);
		display: flex;
		flex-direction: column;
	}

	.preview-topbar {
		display: flex;
		align-items: center;
		padding: 20rpx 30rpx;
		flex-shrink: 0;
	}

	.preview-close {
		font-size: 36rpx;
		color: #ffffff;
		padding: 10rpx;
		width: 60rpx;
		text-align: center;
	}

	.preview-counter {
		font-size: 24rpx;
		color: #94a3b8;
		margin-left: 20rpx;
	}

	.preview-filename {
		font-size: 22rpx;
		color: #64748b;
		margin-left: 20rpx;
		flex: 1;
		text-align: right;
		overflow: hidden;
		text-overflow: ellipsis;
		white-space: nowrap;
	}

	.preview-body {
		flex: 1;
		display: flex;
		align-items: center;
		justify-content: center;
		position: relative;
		overflow: hidden;
	}

	.preview-movable {
		width: 100%;
		height: 100%;
	}

	.preview-movable-view {
		width: 100%;
		height: 100%;
	}

	.preview-image {
		width: 100%;
		height: 100%;
	}

	.preview-zoom-hint {
		position: absolute;
		top: 20rpx;
		left: 50%;
		transform: translateX(-50%);
		display: flex;
		align-items: center;
		gap: 20rpx;
		padding: 10rpx 24rpx;
		background: rgba(0, 0, 0, 0.7);
		border-radius: 20rpx;
		font-size: 22rpx;
		color: #94a3b8;
		z-index: 20;
		pointer-events: auto;
	}

	.preview-zoom-reset {
		color: #60a5fa;
		font-size: 22rpx;
		padding: 6rpx 16rpx;
		background: rgba(96, 165, 250, 0.15);
		border-radius: 10rpx;
	}

	.preview-arrow {
		position: absolute;
		top: 50%;
		transform: translateY(-50%);
		width: 80rpx;
		height: 80rpx;
		display: flex;
		align-items: center;
		justify-content: center;
		background: rgba(255, 255, 255, 0.1);
		border-radius: 50%;
		z-index: 10;
	}

	.preview-arrow text {
		font-size: 48rpx;
		color: #ffffff;
		font-weight: bold;
	}

	.preview-arrow-left {
		left: 20rpx;
	}

	.preview-arrow-right {
		right: 20rpx;
	}

	.preview-bottombar {
		display: flex;
		align-items: center;
		justify-content: space-around;
		padding: 24rpx 30rpx;
		flex-shrink: 0;
		border-top: 1rpx solid rgba(255, 255, 255, 0.08);
	}

	.preview-action {
		font-size: 26rpx;
		color: #cbd5e1;
		padding: 14rpx 36rpx;
		border-radius: 12rpx;
		background: rgba(255, 255, 255, 0.1);
	}

	.preview-action:active {
		background: rgba(255, 255, 255, 0.2);
	}

	.preview-action-delete {
		color: #fca5a5;
	}
</style>