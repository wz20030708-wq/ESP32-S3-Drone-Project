<script>
	export default {
		onLaunch: function() {
			console.log('App Launch')
			// 强制横屏
			// #ifdef APP-PLUS
			plus.screen.lockOrientation('landscape-primary');
			// #endif
		},
		onShow: function() {
			console.log('App Show')
		},
		onHide: function() {
			console.log('App Hide')
		}
	}
</script>

<style>
	/* 全局样式 - 全屏深色背景 */
	page {
		width: 100%;
		height: 100%;
		background-color: #0f0f23;
		overflow: hidden;
	}

	/* 隐藏滚动条 */
	::-webkit-scrollbar {
		display: none;
		width: 0;
		height: 0;
	}
</style>