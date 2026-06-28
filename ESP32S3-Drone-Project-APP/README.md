# ESP32 无人机上位机

基于 uni-app 开发的 ESP32 无人机控制应用，支持实时视频流显示、AI 物体检测和截图管理。

## 项目简介

本项目是一个跨平台的移动端应用，用于连接和控制 ESP32 无人机设备。通过 WebView 实时显示无人机摄像头画面，集成阿里云视觉智能 API 进行物体检测，并提供完整的截图管理功能。

## 核心功能

### 1. 设备连接
- 支持输入设备 IP 地址连接（默认：192.168.4.1）
- 实时显示连接状态和错误提示
- 支持断开连接功能

### 2. 实时视频流
- 通过 WebView 加载无人机摄像头视频流
- 强制横屏显示，优化观看体验
- 支持连接失败提示和错误处理

### 3. 自动截图与 AI 检测
- 每 5 秒自动截取视频画面
- 集成阿里云物体检测 API，支持 4 种检测模式：
  - 全部物体：检测画面中的所有物体
  - 人物：专门检测人物目标
  - 车辆：专门检测车辆目标
  - 动物：专门检测动物目标
- 检测结果实时标注在截图上（红色检测框 + 物体标签）
- 右下角浮层实时显示标注后的截图

### 4. 截图文件管理
- 查看所有截图文件（按时间排序）
- 支持图片预览（支持缩放、拖拽、切换）
- 支持批量选择、删除和保存到相册
- 显示文件信息（名称、日期、大小）

## 技术架构

### 技术栈
- **前端框架**：uni-app + Vue 3
- **跨平台**：支持 Android、iOS、小程序等多端
- **AI 服务**：阿里云视觉智能开放平台（物体检测）
- **加密算法**：纯 JavaScript 实现的 SHA-1、HMAC-SHA1、Base64

### 项目结构
```
ESP32S3-Drone-Project-APP/
├── pages/
│   ├── index/index.vue          # 主页（连接界面）
│   ├── webview/webview.vue      # 视频流与 AI 检测界面
│   └── recordings/recordings.vue # 截图文件管理界面
├── utils/
│   └── aliyun-api.js            # 阿里云 API 调用工具
├── App.vue                      # 应用入口
├── manifest.json                # 应用配置（权限、版本等）
├── pages.json                   # 页面路由配置
└── README.md                    # 项目说明文档
```

### 核心技术实现

#### 1. 阿里云物体检测流程
```
截图 → 上传到 OSS → 获取公开 URL → 调用阿里云 DetectObject API → 解析结果 → 绘制标注框
```

#### 2. 标注绘制技术
- 使用 Android 原生 Canvas API 绘制标注框和文字标签
- 避免依赖 document 对象，兼容 WebView 环境
- 支持实时更新和错误处理

#### 3. 截图技术
- 使用 `plus.nativeObj.Bitmap` 截取 WebView 内容
- 支持指定区域截取（避免截取导航栏等 UI 元素）
- 自动保存到 `_doc/Screenshots/` 目录

## 安装与使用

### 前置要求
- ESP32 无人机设备（已配置摄像头和 Wi-Fi 热点）
- Android 或 iOS 手机
- 阿里云账号（需开通视觉智能服务和 OSS）

### 配置阿里云服务
在 `utils/aliyun-api.js` 中配置以下信息：

```javascript
const CONFIG = {
  accessKeyId: '你的AccessKey ID',
  accessKeySecret: '你的AccessKey Secret',
  regionId: 'cn-shanghai',
  oss: {
    bucket: '你的OSS Bucket名称',
    region: 'cn-shanghai',
    endpoint: 'oss-cn-shanghai.aliyuncs.com',
    folder: 'screenshots'
  }
}
```

### 编译与运行
1. 安装 HBuilderX 开发工具
2. 导入项目到 HBuilderX
3. 运行到手机或模拟器
4. 编译打包生成 APK（已生成：`ESPFly2.0.apk`）

### 使用步骤
1. 打开应用，确保手机 Wi-Fi 已连接到无人机热点
2. 在主页输入无人机 IP 地址（默认：192.168.4.1）
3. 点击"连接"按钮进入视频流界面
4. 应用将自动开始截图和 AI 检测
5. 右下角浮层实时显示标注后的截图
6. 点击"截图文件"查看和管理所有截图

## 应用权限

应用需要以下权限：
- **INTERNET**：网络连接
- **CAMERA**：摄像头访问
- **WRITE_EXTERNAL_STORAGE**：写入外部存储
- **READ_EXTERNAL_STORAGE**：读取外部存储
- **RECORD_AUDIO**：录音权限
- **ACCESS_WIFI_STATE**：访问 Wi-Fi 状态

## 版本信息

- **应用名称**：ESP32Fly
- **版本号**：2.0.0
- **版本代码**：100
- **应用 ID**：__UNI__37ECE35

## 特色功能

### 1. 智能检测模式切换
支持一键切换物体检测模式（全部物体、人物、车辆、动物），通过客户端过滤实现分类检测。

### 2. 实时标注显示
检测结果实时标注在截图上，包括：
- 红色检测框标注物体位置
- 文字标签显示物体名称和置信度
- 支持多个物体同时标注

### 3. 截图文件管理
完整的文件管理功能：
- 支持图片预览、缩放、拖拽
- 支持批量选择、删除、保存到相册
- 显示文件详细信息

### 4. 横屏优化设计
- 强制横屏显示，优化无人机观看体验
- 所有界面采用横屏布局设计
- 深色背景配色，减少视觉疲劳

## 已编译版本

项目已编译生成 APK 文件：
- **文件路径**：`unpackage/release/apk/ESPFly2.0.apk`
- **可直接安装使用**

## 技术文档

详细的技术实现文档请参考：
- [修复标注绘制技术方案](.trae/documents/fix-annotations-bitmap.md)

## 开发者说明

### 加密算法实现
项目使用纯 JavaScript 实现加密算法（SHA-1、HMAC-SHA1、Base64），不依赖任何第三方库，确保跨平台兼容性。

### Android 原生 API 使用
标注绘制功能使用 Android 原生 Canvas API，包括：
- `android.graphics.BitmapFactory`
- `android.graphics.Canvas`
- `android.graphics.Paint`
- `android.graphics.Color`

### 错误处理
- 截图失败自动跳过并继续下一次截图
- AI 检测失败仍显示原始截图
- 连接失败提供详细的错误提示

## 许可证

本项目仅供学习和研究使用。

## 联系方式

如有问题或建议，请通过项目仓库提交 Issue。