# 修复标注绘制：用 Native Bitmap API 替代 document.createElement

## 问题根因

`document.createElement` 在 Android WebView 中为 `undefined`，导致 `_drawAnnotations` 的 Promise 挂起，`isDetecting` 永远为 `true`，后续截图全部跳过检测。

## 修改方案

将 `_drawAnnotations` 完全重写，使用 `plus.nativeObj.Bitmap` 原生 API 绘制标注框和文字，彻底避免对 `document` 的依赖。

### 修改文件

`pages/webview/webview.vue` — 替换 `_drawAnnotations` 方法

### 新流程

```
1. new plus.nativeObj.Bitmap('anno_xxx')  创建新 Bitmap
2. annoBitmap.load(imagePath)             加载已保存的截图文件
3. annoBitmap.drawRect()                  绘制每个检测框
4. annoBitmap.drawText()                  绘制每个标签
5. annoBitmap.save(annotatedPath)         保存标注图片
6. annoBitmap.clear()                     清理
```

### 关键技术点

1. **坐标无需缩放**：`annoBitmap.load(imagePath)` 加载的文件与上传到 OSS 的文件相同，API 返回的坐标直接对应 Bitmap 坐标
2. **drawRect 使用**：`Bitmap.drawRect(color, {top, left, width, height}, lineWidth)`
3. **drawText 使用**：`Bitmap.drawText(text, {color, size, align}, x, y)`
4. **错误处理**：每个回调都包含 reject，确保 Promise 一定会 resolve 或 reject

### 代码改动

完全替换 `_drawAnnotations` 方法，约 60 行新代码。

### 验证方式

1. 运行应用，连接设备
2. 观察控制台日志，应看到 `[标注] 使用 Native Bitmap 绘制...` 和 `[标注] 标注图片已保存`
3. 右下角浮层应显示带红色标注框的图片
4. 后续截图应继续触发检测（不再卡在 `isDetecting`）