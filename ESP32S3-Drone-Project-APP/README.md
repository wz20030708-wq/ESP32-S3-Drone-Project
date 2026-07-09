# ESP32Fly — ESP32-S3 无人机上位机 APP

> 版本：2.0.0 | 应用 ID：`__UNI__37ECE35`

基于 **uni-app（Vue 3）** 框架开发的跨平台移动端应用，用于连接和控制 **ESP32-S3 无人机**。通过 WebView 实时显示无人机摄像头画面，集成阿里云视觉智能 API 进行 AI 物体检测，并提供截图文件管理等功能。

---

## 目录

- [功能特性](#功能特性)
- [技术栈](#技术栈)
- [项目结构](#项目结构)
- [快速开始](#快速开始)
  - [环境准备](#环境准备)
  - [运行项目](#运行项目)
  - [构建 APK](#构建-apk)
- [页面说明](#页面说明)
  - [设备连接页](#设备连接页-pagesindexindex)
  - [视频流与 AI 检测页](#视频流与-ai-检测页-pageswebviewwebview)
  - [截图文件管理页](#截图文件管理页-pagesrecordingrecordings)
- [核心模块详解](#核心模块详解)
  - [阿里云 API 封装](#阿里云-api-封装-utilsaliyun-apijs)
  - [工具模块](#工具模块-utils)
  - [测试套件](#测试套件-test)
- [应用权限](#应用权限)
- [设计特点](#设计特点)
- [常见问题](#常见问题)
- [许可证](#许可证)

---

## 功能特性

- **实时视频流** — 通过 WebView 加载无人机摄像头 HTTP 视频流，实时查看飞行画面
- **AI 物体检测** — 集成阿里云视觉智能 DetectObject API，自动识别画面中的物体
  - 支持 4 种检测模式：全部物体 / 人物 / 车辆 / 动物
  - 在图片上绘制红色标注框和标签
  - 实时展示检测结果与置信度
- **自动截图** — 每 5 秒自动截取 WebView 画面并保存至本地
- **检测结果标注** — 使用 Android 原生 Canvas API 在截图图片上绘制检测框
- **截图文件管理** — 浏览、预览（支持缩放拖拽）、批量删除、批量保存到相册
- **全横屏体验** — 所有页面强制横屏，深色主题 UI
- **零第三方依赖** — 所有加密算法（SHA-1、HMAC-SHA1）纯 JavaScript 实现，无需额外库

---

## 技术栈

| 类别 | 技术/库 | 说明 |
|------|---------|------|
| **前端框架** | uni-app (Vue 3) | 跨平台应用框架，一套代码打包 Android/iOS/小程序 |
| **构建工具** | HBuilderX | uni-app 官方 IDE |
| **UI 样式** | SCSS + CSS3 | 深色主题、渐变背景、横屏适配 |
| **AI 服务** | 阿里云视觉智能开放平台 | DetectObject 物体检测 API |
| **云存储** | 阿里云 OSS | 截图图片上传与临时存储 |
| **加密算法** | 纯 JavaScript 实现 | SHA-1、HMAC-SHA1（Base64）自实现，无第三方依赖 |
| **原生 API** | 5+ App（HTML5 Plus） | Android Canvas 绘制标注框、Bitmap 截图、文件系统操作 |
| **测试框架** | Node `node:test` + `node:assert/strict` | ESM 测试套件，无需第三方测试库 |
| **性能基准** | Node `perf_hooks` | 内置性能测量工具 |

---

## 项目结构

```
ESP32S3-Drone-Project-APP/
├── App.vue                         # 全局应用入口（强制横屏、深色背景）
├── main.js                         # Vue 应用启动入口
├── index.html                      # H5 模式入口 HTML
├── manifest.json                   # 应用清单（权限、图标、横屏配置、版本等）
├── pages.json                      # 页面路由配置
├── uni.scss                        # uni-app 全局样式变量
├── uni.promisify.adaptor.js        # uni-app Promise 适配器
├── README.md                       # 本文件
├── logo.png                        # 应用图标
│
├── pages/                          # 页面目录
│   ├── index/
│   │   └── index.vue               # 主页 - 设备连接界面
│   ├── webview/
│   │   └── webview.vue             # 视频流 + AI 检测界面（核心页面）
│   └── recordings/
│       └── recordings.vue          # 截图文件管理界面
│
├── utils/                          # 工具模块
│   ├── aliyun-api.js               # 阿里云 API 调用（加密/签名/OSS/物体检测）
│   ├── format.js                   # 格式化工具（时间戳、文件大小）
│   ├── log.js                      # 分级日志工具
│   └── package.json                # ES Module 声明
│
├── test/                           # 单元测试目录
│   ├── crypto.test.mjs             # 加密算法测试（SHA-1、HMAC-SHA1）
│   ├── parse.test.mjs              # 阿里云响应解析测试
│   ├── encode.test.mjs             # RFC3986 URL 编码测试
│   ├── format.test.mjs             # 格式化函数测试
│   └── benchmark.mjs               # 性能基准测试
│
├── static/                         # 静态资源
│   └── logo.png
│
└── unpackage/                      # 构建产物目录
    └── dist/dev/app-plus/          # App 端开发构建产物
```

---

## 快速开始

### 环境准备

1. **安装 HBuilderX** — 前往 [官方下载页](https://www.dcloud.io/hbuilderx.html) 下载并安装 HBuilderX
2. **配置阿里云凭证**（可选，用于 AI 检测功能）
   - 在 `utils/aliyun-api.js` 中配置以下参数：
     - `accessKeyId` — 阿里云 AccessKey ID
     - `accessKeySecret` — 阿里云 AccessKey Secret
     - `ossBucket` — OSS Bucket 名称
     - `ossRegion` — OSS Bucket 所在地域

### 运行项目

1. 使用 HBuilderX 打开项目根目录
2. 选择运行目标：
   - **Android 真机**：菜单栏 → 运行 → 运行到手机或模拟器 → 选择设备
   - **H5 浏览器**：菜单栏 → 运行 → 运行到浏览器
3. 确保手机与 ESP32 无人机连接同一 Wi-Fi 网络（或连接无人机自建热点）

### 构建 APK

1. HBuilderX → 发行 → 原生 App-云打包
2. 配置包名 `__UNI__37ECE35` 及版本号
3. 等待云打包完成，产物位于 `unpackage/release/apk/`
4. 也可使用已编译的 APK：`unpackage/release/apk/ESPFly2.0.apk`

---

## 页面说明

### 设备连接页（`pages/index/index.vue`）

首页为设备连接界面，用户输入 ESP32 无人机的 IP 地址后点击连接。

- 默认 IP：`192.168.4.1`（ESP32 AP 模式默认地址）
- 支持 IP 格式校验
- 连接后跳转至视频流页面
- 从视频流页返回时自动重置连接状态

### 视频流与 AI 检测页（`pages/webview/webview.vue`）

项目核心页面，集成了视频显示、自动截图和 AI 物体检测三大功能。

**视频流：**
- 通过 `<web-view>` 加载 `http://<设备IP>` 的摄像头视频流
- 页面底部提供"断开连接"按钮

**自动截图与 AI 检测流程：**
1. 每 5 秒自动截取 WebView 画面（裁剪避开 UI 元素）
2. 保存截图至 `_doc/Screenshots/` 目录，文件名 `SS_YYYYMMDDHHmmss.png`
3. 上传截图至阿里云 OSS 获取公开 URL
4. 调用阿里云 DetectObject API 进行物体检测
5. 使用 Android 原生 Canvas API 在截图上绘制红色检测框和标签
6. 右下角浮层实时显示标注后的截图

**检测模式切换：**
- 全部物体
- 人物
- 车辆
- 动物

### 截图文件管理页（`pages/recordings/recordings.vue`）

管理所有自动截图的文件。

- 文件列表按时间倒序排列，显示文件名、日期、大小
- 图片预览：支持 1x~4x 缩放、拖拽、左右切换
- 批量操作：长按进入多选模式，支持全选、批量删除、批量保存到相册

---

## 核心模块详解

### 阿里云 API 封装（`utils/aliyun-api.js`）

约 445 行，是项目最核心的工具模块，包含：

- **SHA-1 哈希算法** — 严格遵循 RFC 3174 标准的纯 JS 实现
- **HMAC-SHA1 签名算法** — Base64 输出，用于阿里云 API 认证
- **RFC3986 URL 编码** — 完整的百分号编码实现
- **OSS 表单上传** — POST + Policy 签名，支持文件上传至云端
- **DetectObject API 调用** — RESTful 签名认证与请求
- **4 种检测模型** — 内置模型列表与类别过滤器
- **检测结果解析器** — 解析 API 响应，提取物体名称与置信度

关键导出函数：

| 函数 | 说明 |
|------|------|
| `sha1(message)` | 计算 SHA-1 哈希，返回十六进制字符串 |
| `hmacSha1Base64(key, data)` | HMAC-SHA1 签名，返回 Base64 编码 |
| `encode(str)` | RFC3986 百分号编码 |
| `detectLocalImage(filePath, model)` | 上传图片并执行物体检测 |
| `parseDetectionResult(result)` | 解析检测 API 返回的数据 |
| `getModelList()` | 获取所有可用检测模型 |
| `setModel(modelId)` | 切换当前检测模型 |

### 工具模块（`utils/`）

| 文件 | 功能说明 |
|------|----------|
| `format.js` | `formatTimestamp` — 日期转 14 位数字（YYYYMMDDHHmmss）；`formatFileSize` — 字节数转可读格式（B/KB/MB） |
| `log.js` | 分级日志工具，提供 `debug/info/warn/error` 四种级别，debug 级别受全局开关控制 |

### 测试套件（`test/`）

全部使用 Node 内置 API，无需安装任何第三方测试库。

运行测试：

```bash
# 运行所有测试
node --test test/*.test.mjs

# 运行单个测试
node --test test/crypto.test.mjs
```

| 测试文件 | 测试内容 | 测试用例数 |
|----------|----------|-----------|
| `crypto.test.mjs` | SHA-1 3 个 RFC 标准向量 + HMAC-SHA1 RFC 2202 向量 | 4 |
| `encode.test.mjs` | RFC3986 编码 6 个场景 | 6 |
| `parse.test.mjs` | 阿里云响应解析 5 个场景 | 5 |
| `format.test.mjs` | 时间戳 + 文件大小 9 个边界值 | 9 |
| `benchmark.mjs` | 性能基准测试（模块加载/编码/格式化等） | - |

---

## 应用权限

应用在 `manifest.json` 中声明了以下 Android 权限：

| 权限 | 用途 |
|------|------|
| `INTERNET` | 网络连接，访问无人机设备和阿里云 API |
| `CAMERA` | 摄像头权限 |
| `RECORD_AUDIO` | 录音权限 |
| `WRITE_EXTERNAL_STORAGE` | 写入外部存储，保存截图文件 |
| `READ_EXTERNAL_STORAGE` | 读取外部存储，浏览截图文件 |
| `ACCESS_WIFI_STATE` | Wi-Fi 状态管理，连接无人机 |
| `CHANGE_WIFI_STATE` | 切换 Wi-Fi 连接 |
| `FOREGROUND_SERVICE` | 前台服务 |
| `FOREGROUND_SERVICE_MEDIA_PROJECTION` | 媒体投影前台服务 |
| `VIBRATE` / `FLASHLIGHT` / `WAKE_LOCK` | 辅助功能 |

---

## 设计特点

1. **全横屏体验** — 所有页面通过 `pageOrientation: "landscape"` 强制横屏，并在应用启动时通过 `plus.screen.lockOrientation('landscape-primary')` 锁定方向
2. **深色主题** — 全局背景 `#0f0f23`，深蓝渐变配色，适合飞行场景使用
3. **零第三方依赖** — 所有加密算法（SHA-1、HMAC-SHA1）均为纯 JavaScript 手写实现
4. **Android 原生深度集成** — 直接使用 Java 反射 API 操作 Bitmap、Canvas、File 等 Android 原生对象
5. **完善测试覆盖** — 内置完整的单元测试和性能基准测试，覆盖核心加密、编码、解析逻辑
6. **Vue 2 / Vue 3 双版本兼容** — 通过 uni-app 条件编译指令支持双版本运行时

---

## 常见问题

**Q: 无法连接无人机？**
A: 确保手机已连接至无人机 Wi-Fi（ESP32 默认 SSID），默认 IP 为 `192.168.4.1`。

**Q: AI 检测功能无法使用？**
A: 需在 `utils/aliyun-api.js` 中配置有效的阿里云 AccessKey 和 SecretKey，并确保已开通视觉智能开放平台服务。

**Q: 截图保存失败？**
A: 请确认应用已获取存储读写权限，可在系统设置中手动授权。

**Q: 视频流加载缓慢？**
A: 检查 Wi-Fi 信号强度，确保无人机与手机之间无遮挡。也可在 ESP32 端调整视频流参数。

---

