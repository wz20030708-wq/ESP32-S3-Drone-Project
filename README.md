# ESP32-S3 四旋翼无人机完整解决方案（飞控固件 + UniApp 移动端上位机）
整合文档：Flix飞控固件(ESP32-S3) + ESP32Fly 移动端APP，完整软硬件一体化方案，内容完整保留、排版分层美化

## 目录总览
1. 🚁 Flix 飞控 — ESP32-S3 四旋翼无人机固件
2. 📱 ESP32Fly — ESP32-S3 无人机移动端上位机APP
3. 整机联动交互说明（飞控+APP配合流程）

---

# 第一部分 🚁 Flix 飞控 — ESP32-S3 四旋翼无人机固件
**基于 FreeRTOS 双核多任务架构完整四旋翼飞行控制系统**

| 标签标识 | 内容 |
| ---- | ---- |
| MCU | ESP32-S3 |
| 开发框架 | Arduino |
| 实时系统 | FreeRTOS |
| 地面站协议 | MAVLink |

[🧬 项目概述](#1-1-项目概述) • [📦 硬件引脚](#1-2-硬件引脚定义) • [🏗️ 系统架构](#1-3-系统架构) • [🎮 飞行控制](#1-4-飞行控制) • [📡 通信方式](#1-5-通信方式) • [🔧 构建指南](#1-6-构建指南) • [⚙️ 使用说明](#1-7-使用说明)

## 1.1 项目概述
Flix 飞控是基于 ESP32-S3 的四旋翼无人机固件，覆盖专业飞控全部核心能力：

| 功能模块 | 详细说明 |
| ---- | ---- |
| 姿态估计 | 互补滤波 / Madgwick AHRS 双算法可切换 |
| 控制架构 | 四层级联PID：位置→速度→姿态角→角速度 |
| 飞行模式 | 增稳模式(STAB)、悬停模式(HOVER) |
| 遥控接收 | SBUS接收机，支持自动通道校准 |
| 地面站对接 | MAVLink over UDP，兼容QGroundControl |
| 图传系统 | OV3660摄像头，JPEG网页实时预览 |
| 传感器组 | MPU6500九轴IMU + BMP280气压计 + VL53L0X激光测距 |
| 数据记录 | RAM环形日志，100Hz采样，循环存储10秒飞行数据 |
| 故障保护 | RC遥控信号丢失自动平缓降落 |
| 参数管理 | NVS闪存持久化，飞行中在线调参 |

## 1.2 硬件引脚定义
### 1.2.1 SPI总线 - MPU9250 IMU
| 信号 | GPIO | 功能说明 |
| ---- | ---- | ---- |
| SCK | 38 | SPI时钟 |
| MISO | 36 | SPI输入 |
| MOSI | 37 | SPI输出 |
| CS | 35 | 片选引脚 |

### 1.2.2 I2C总线（400kHz Fast Mode）
挂载设备：BMP280(0x76)、VL53L0X(0x29)
| 信号 | GPIO | 功能说明 |
| ---- | ---- | ---- |
| SDA | 47 | I2C数据 |
| SCL | 21 | I2C时钟 |

### 1.2.3 电机PWM输出（LEDC驱动）
| 电机编号 | GPIO | 机身位置 |
| ---- | ---- | ---- |
| M0 | 41 | 后左 |
| M1 | 40 | 后右 |
| M2 | 39 | 前右 |
| M3 | 42 | 前左 |
- 直流有刷MOS驱动：20kHz，10位分辨率
- 无刷ESC驱动备选：400Hz，16位分辨率

### 1.2.4 OV3660摄像头（ESP32S3-EYE标准并行接口）
| 信号 | GPIO | 信号 | GPIO |
| ---- | ---- | ---- | ---- |
| PCLK | 13 | VSYNC | 6 |
| HREF | 7 | XCLK | 15 |
| SDA | 4 | SCL | 5 |
| D0~D3 | 11,9,8,10 | D4~D7 | 12,18,17,16 |

### 1.2.5 其余外设引脚
| 外设功能 | GPIO | 参数说明 |
| ---- | ---- | ---- |
| SBUS遥控接收 | 14(UART2 RX) | SBUS协议 |
| 电机电压检测 | 1(ADC1_CH0) | 12位ADC，11dB衰减 |
| 芯片供电电压检测 | 2(ADC1_CH1) | 12位ADC，11dB衰减 |
| 板载状态LED | 48 | 运行状态指示灯 |

## 1.3 系统架构
### 1.3.1 FreeRTOS双核三任务调度（ESP32-S3双核）
| 任务名称 | 运行核心 | 优先级 | 栈大小 | 运行频率 | 核心职责 |
| ---- | ---- | ---- | ---- | ---- | ---- |
| FlightCtrl 飞控主任务 | Core1 | 24（最高） | 8KB | ~1kHz | IMU读取、RC解析、姿态解算、PID控制、电机输出 |
| CommTask 通信任务 | Core0 | 5（中等） | 8KB | ~200Hz | 串口CLI、Web服务、MAVLink地面站通信 |
| BgTask 后台任务 | Core0 | 1（最低） | 4KB | ~100Hz | 电压采集、辅助传感器、摄像头、日志、LED、参数同步 |

### 1.3.2 项目文件分层结构
```
RC_QGC_photo_wifi/
├── RC_QGC_photo_wifi.ino        # 程序入口：初始化、任务创建、CPU监控
├── globals.h                    # 全局变量声明
│
├─ 飞行控制模块
│   ├── control.ino    四级PID控制、电机混控、悬停算法
│   ├── estimate.ino   互补滤波/Madgwick姿态解算
│   ├── motors.ino     四路PWM电机驱动
│   ├── imu.ino        MPU6500 SPI驱动
│   ├── rc.ino         SBUS遥控解析、通道校准
│   ├── sensors.ino    I2C气压计+激光测距
│   ├── safety.ino     失控保护逻辑
│   └── time.ino       全局计时、循环频率统计
│
├─ 通信模块
│   ├── wifi.ino       WiFi STA/AP、UDP、网页监控
│   ├── mavlink.ino    MAVLink v2 地面站协议
│   └── cli.ino        串口调试命令行
│
├─ 外设驱动模块
│   ├── camera.ino     OV3660图传驱动
│   ├── led.ino        状态灯控制
│   └── voltage.ino    ADC电压采集
│
├─ 数据存储模块
│   ├── parameters.ino NVS闪存参数持久化
│   └── log.ino        环形飞行日志
│
└─ 数学算法库
    ├── vector.h       三维向量运算
    ├── quaternion.h   四元数旋转
    ├── pid.h          PID控制器封装
    ├── lpf.h          低通/陷波滤波器
    ├── madgwick.h     Madgwick AHRS算法
    ├── hover_controller.h 级联悬停控制器
    └── util.h         通用工具函数
```

## 1.4 飞行控制逻辑
### 1.4.1 四层级联PID控制链路
`摇杆指令 → 位置P环 → 速度PID环 → 姿态角PID环 → 角速度PID环 → 电机混控输出`

| 控制层级 | 控制器类型 | 控制轴数量 | 作用 |
| ---- | ---- | ---- | ---- |
| 位置环 | 单P | 2轴（水平XY） | 定点水平位置控制 |
| 速度环 | PID | 2轴（水平XY） | 水平速度稳定，支持加速度前馈 |
| 姿态角环 | PID | 3轴（横滚/俯仰/偏航） | 保持机身水平姿态 |
| 角速度环 | PID | 3轴（横滚/俯仰/偏航） | 高速角速度稳定，抑制抖动 |

### 1.4.2 两种飞行模式
1. **STAB 增稳模式**
    - 横滚/俯仰摇杆：直接输出目标姿态角（±30°）
    - 偏航摇杆：控制机身旋转角速度
    - 松开摇杆自动回平
2. **HOVER 悬停模式**
    - 横滚/俯仰摇杆：控制水平飞行速度（最大1.5m/s）
    - 油门居中自动定高；油门偏移控制垂直速度（最大1.0m/s）
    - 偏航摇杆控制旋转角速度，松杆自动定点悬停

### 1.4.3 姿态解算算法
- **默认：互补滤波**：陀螺仪积分姿态 + 加速度计重力向量修正，飞行时自动降低修正权重，避免机动干扰
- **可选：Madgwick AHRS**：梯度下降六轴融合，开启宏 `#define USE_MADGWICK_FILTER` 启用

### 1.4.4 安全保护机制
1. 低油门积分清零：油门输出＜0.12时清空PID积分，防止积分饱和
2. RC信号丢失保护：1秒无遥控信号触发自动降落，10秒平滑减速落地
3. 电压跌落防复位：关闭芯片BrownOut复位功能
4. 模式切换防抖：连续3次采样确认才切换飞行模式
5. 数值校验：全链路过滤NAN/INF非法数据，防止飞控失控

## 1.5 通信交互方式
### 1.5.1 Web网页监控（默认地址：192.168.1.100）
页面功能：
- OV3660实时视频流（150ms刷新）
- 实时姿态、高度、电池电压、CPU占用展示
- 四路电机输出可视化进度条
- 整机运行状态仪表盘

### 1.5.2 MAVLink UDP地面站（端口14550，适配QGroundControl）
- 实时姿态、高度、速度数据上报
- 飞行PID参数在线读写调参
- 飞行器解锁/上锁、飞行模式远程切换

### 1.5.3 串口CLI调试（波特率115200）
常用调试命令：

| 命令 | 功能 |
| ---- | ---- |
| help | 查看全部命令说明 |
| info | 整机运行状态总览 |
| calib_acc | 加速度计六面校准（约48秒） |
| calib_gyro | 陀螺仪零偏校准 |
| param list | 打印全部存储参数 |
| param set [name] [val] | 在线修改PID/控制参数 |
| log dump | 导出环形飞行日志 |
| motor test n | 单独测试指定电机 |
| reboot | 飞控重启 |

## 1.6 构建编译指南
### 1.6.1 硬件清单
| 器件 | 型号 | 数量 |
| ---- | ---- | ---- |
| 主控 | ESP32-S3开发板 | 1 |
| IMU | MPU6500 SPI版本 | 1 |
| 气压计 | BMP280 I2C | 1 |
| 激光测距 | VL53L0X | 1 |
| 摄像头 | OV3660 | 1 |
| 动力 | 直流有刷电机/无刷电机+ESC ×4 | 4 |
| 遥控接收机 | SBUS协议接收机 | 1 |

### 1.6.2 依赖库列表
| 库名称 | 用途 | 安装渠道 |
| ---- | ---- | ---- |
| FlixPeriph | MPU6500 SPI驱动 | Arduino库管理器 |
| SBUS | SBUS遥控协议解析 | Arduino库管理器 |
| MAVLink | MAVLink v2协议栈 | Arduino库管理器 |
| VL53L0X | 激光测距传感器驱动 | Arduino库管理器 |
| esp_camera | OV3660摄像头 | ESP32 Arduino内置 |
| WiFi/WebServer/WiFiUdp | 无线网络通信 | ESP32 Arduino内置 |
| SPI/Wire | 总线驱动 | ESP32 Arduino内置 |
| Preferences | NVS闪存存储 | ESP32 Arduino内置 |

### 1.6.3 编译配置
#### Arduino IDE编译步骤
1. 安装ESP32 Arduino Core（版本≥2.0.x）
2. 库管理器安装全部第三方依赖库
3. 开发板选择：ESP32S3 Dev Module
4. PSRAM配置：OPI PSRAM
5. 编译上传固件

#### PlatformIO 配置模板
```ini
[env:esp32-s3-dev]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps =
    FlixPeriph
    SBUS
    MAVLink
    VL53L0X
board_build.arduino.memory_type = qio_opi
```

#### 全局编译宏配置（文件头部）
| 宏定义 | 默认值 | 作用 |
| ---- | ---- | ---- |
| WIFI_ENABLED | 1 | 开启WiFi、MAVLink、网页图传 |
| CAMERA_ENABLED | 1 | 启用OV3660摄像头图传 |
| USE_MADGWICK_FILTER | 未定义 | 启用Madgwick姿态解算算法 |

## 1.7 使用操作流程
1. **硬件接线**：严格按照引脚定义连接所有传感器、电机、遥控、摄像头
2. **固件烧录**：完成编译并上传至ESP32-S3主控
3. **上电初始化**：板载LED快速闪烁代表硬件初始化运行中
4. **传感器校准**：串口CLI输入`calib_acc`执行加速度计六面校准
5. **地面站连接**：QGroundControl自动识别`192.168.1.100:14550`设备
6. **试飞操作**：解锁飞行器→缓慢推油门离地起飞

### 参数与WiFi配置
1. PID在线调参示例（串口/MAVLink通用）
```
param set roll_rate_p 0.15
param set roll_rate_i 0.01
param set roll_rate_d 0.002
param list
```
2. WiFi参数配置（wifi.ino）
```cpp
#define WIFI_SSID      "你的WiFi名称"
#define WIFI_PASSWORD  "你的WiFi密码"
#define WIFI_STATIC_IP "192.168.1.100"
```

---

# 第二部分 📱 ESP32Fly — ESP32-S3 无人机移动端上位机APP
基于 uni-app(Vue3) 跨平台移动端APP，配套Flix飞控使用，网页图传显示+AI视觉物体检测一体化上位机

[功能特性](#2-1-功能特性) • [技术栈](#2-2-技术栈) • [项目结构](#2-3-项目结构) • [快速部署](#2-4-快速开始) • [页面说明](#2-5-页面详情) • [核心模块](#2-6-核心模块详解) • [权限与设计](#2-7-权限/设计特点) • [常见问题](#2-8-故障排查)

## 2.1 功能特性
1. **实时无人机视频流**：WebView加载飞控HTTP网页图传，实时查看飞行画面
2. **阿里云AI物体检测**：DetectObject视觉API，4种检测模式切换
   - 全部物体 / 人物 / 车辆 / 动物
   - 画面自动绘制红色标注框+类别+置信度
3. **自动截图存储**：每5秒自动截取画面，本地持久保存
4. 截图管理：预览缩放、批量删除、批量保存至手机相册
5. 全页面强制横屏，深色飞行主题UI
6. 加密算法纯JS手写实现，无第三方依赖（SHA-1、HMAC-SHA1）

## 2.2 全套技术栈
| 分类 | 技术/工具 | 用途说明 |
| ---- | ---- | ---- |
| 前端框架 | uni-app(Vue3) | 一套代码打包Android/iOS/H5 |
| IDE工具 | HBuilderX | uni-app官方开发、打包工具 |
| 样式 | SCSS+CSS3 | 深色横屏适配界面 |
| AI云端服务 | 阿里云视觉智能开放平台 | 物体检测API |
| 云存储 | 阿里云OSS | 截图图片临时上传存储 |
| 加密算法 | 原生JavaScript | 自研SHA-1、HMAC-SHA1签名 |
| 原生能力 | 5+ HTML5 Plus | Android Canvas绘图、截图、文件管理 |
| 测试套件 | Node内置test | 单元测试、性能基准测试 |

## 2.3 APP项目完整目录结构
```
ESP32S3-Drone-Project-APP/
├── App.vue                 # 全局入口：强制横屏、深色主题
├── main.js                 # Vue应用启动文件
├── index.html              # H5模式入口页面
├── manifest.json           # APP权限、包名、版本配置
├── pages.json              # 页面路由配置
├── uni.scss                # 全局样式变量
├── logo.png                # APP图标
│
├─ pages/ 页面目录
│   ├── index/index.vue     # 首页：无人机IP连接页面
│   ├── webview/webview.vue # 核心页：视频流+AI检测
│   └── recordings/recordings.vue # 截图文件管理页
│
├─ utils/ 工具封装
│   ├── aliyun-api.js       # 阿里云签名、OSS、AI检测核心模块
│   ├── format.js           # 时间、文件大小格式化工具
│   └── log.js              # 分级日志打印工具
│
├─ test/ 单元测试
│   ├── crypto.test.mjs     # SHA1/HMAC签名算法测试
│   ├── parse.test.mjs      # AI检测返回数据解析测试
│   ├── encode.test.mjs     # RFC3986 URL编码测试
│   ├── format.test.mjs     # 格式化函数边界测试
│   └── benchmark.mjs       # 性能基准测试
│
├─ static/ 静态资源
└─ unpackage/ 编译产物目录
    └── dist/dev/app-plus/ 开发打包产物
```

## 2.4 快速部署运行
### 2.4.1 环境准备
1. 安装官方HBuilderX开发工具
2. （可选AI检测）配置阿里云凭证：`utils/aliyun-api.js`
   - accessKeyId、accessKeySecret、ossBucket、ossRegion

### 2.4.2 项目运行
1. HBuilderX打开项目根目录
2. 运行目标可选：Android真机 / 浏览器H5
3. 手机与ESP32无人机连接同一WiFi网络

### 2.4.3 APK云打包发布
1. HBuilderX → 发行 → 原生App云打包
2. 配置包名`__UNI__37ECE35`、版本号
3. 打包完成后在`unpackage/release/apk/`获取安装包
4. 预编译成品：`ESPFly2.0.apk`

## 2.5 页面详情说明
### 2.5.1 设备连接页（pages/index/index.vue）
- 用户输入ESP32飞控IP地址建立连接
- AP模式默认IP：`192.168.4.1`，支持IP格式合法性校验
- 连接成功自动跳转视频流页面，返回自动重置连接状态

### 2.5.2 视频流+AI检测核心页（pages/webview/webview.vue）
1. 视频展示：WebView加载飞控网页`http://设备IP`实时图传
2. 自动化截图+AI检测完整流程
    1. 每5秒自动截取画面，裁剪隐藏UI控件
    2. 本地保存截图（命名规则：SS_年月日时分秒.png）
    3. 图片上传阿里云OSS获取公开访问链接
    4. 调用DetectObject API识别画面物体
    5. Android原生Canvas绘制红色检测框、物体标签、置信度
    6. 右下角浮窗实时展示标注后效果图
3. 检测模式切换：全部物体 / 人 / 车辆 / 动物

### 2.5.3 截图文件管理页（pages/recordings/recordings.vue）
- 文件列表按拍摄时间倒序，展示文件名、时间、文件大小
- 图片预览：支持1~4倍缩放、拖拽、左右切换图片
- 批量操作：多选、全选、批量删除、批量保存至手机相册

## 2.6 核心模块详解
### 2.6.1 阿里云API封装（aliyun-api.js，445行核心代码）
自研签名加密，适配阿里云开放平台鉴权规则

| 导出函数 | 功能 |
| ---- | ---- |
| sha1(message) | RFC3174标准SHA1哈希，返回十六进制字符串 |
| hmacSha1Base64(key, data) | HMAC-SHA1签名，输出Base64 |
| encode(str) | RFC3986标准URL百分号编码 |
| detectLocalImage(filePath, model) | 上传图片并执行AI物体检测 |
| parseDetectionResult(result) | 解析API返回检测数据 |
| getModelList() | 获取全部4种检测模型 |
| setModel(modelId) | 切换当前AI检测模式 |

### 2.6.2 工具工具库
1. format.js
   - formatTimestamp：时间戳转14位数字日期
   - formatFileSize：字节自动转为B/KB/MB可读格式
2. log.js：分级日志（debug/info/warn/error），可全局关闭调试日志

### 2.6.3 单元测试套件（Node内置API，无第三方依赖）
执行测试命令
```bash
# 全部测试
node --test test/*.test.mjs
# 单独加密算法测试
node --test test/crypto.test.mjs
```

| 测试文件 | 测试覆盖场景 |
| ---- | ---- |
| crypto.test.mjs | SHA1、HMAC标准向量校验 |
| encode.test.mjs | URL编码边界场景 |
| parse.test.mjs | AI返回数据解析 |
| format.test.mjs | 时间、文件大小格式化边界值 |
| benchmark.mjs | 代码性能基准测试 |

## 2.7 APP权限与设计特点
### 2.7.1 Android系统权限清单（manifest.json）
| 权限 | 使用场景 |
| ---- | ---- |
| INTERNET | 访问无人机WiFi、阿里云API/OSS |
| CAMERA / RECORD_AUDIO | 多媒体辅助权限 |
| WRITE/READ_EXTERNAL_STORAGE | 截图本地存储、相册读写 |
| ACCESS_WIFI_STATE / CHANGE_WIFI_STATE | 切换WiFi连接无人机 |
| FOREGROUND_SERVICE | 后台持续运行 |
| VIBRATE/FLASHLIGHT/WAKE_LOCK | 辅助功能 |

### 2.7.2 设计优势
1. 全局强制横屏，飞行场景操作更舒适
2. 深色深蓝渐变主题，户外强光可视性更好
3. 加密算法全手写，无外部第三方JS依赖，体积更小
4. 深度集成Android原生Canvas、Bitmap，绘图性能高
5. 完整单元测试覆盖核心加密、解析逻辑，稳定性强
6. 兼容Vue2/Vue3双运行时，适配不同uni-app版本

## 2.8 常见故障排查
1. **无法连接无人机**
   手机必须连接无人机WiFi热点，默认AP IP：192.168.4.1；核对飞控WiFi配置
2. **AI检测功能失效**
   阿里云AccessKey/Secret配置错误，或未开通视觉智能开放平台服务
3. **截图保存失败**
   系统设置手动授予APP存储读写权限
4. **视频流卡顿**
   缩短手机与无人机距离，减少遮挡；调整ESP32飞控图传帧率参数

---

# 第三部分 整机联动完整工作流
## 3.1 硬件通信链路
ESP32-S3飞控开启WiFi → 手机连接同一WiFi/无人机热点 → APP输入飞控IP建立通信
1. APP WebView拉取飞控OV3660实时JPEG图传页面
2. APP每5秒截图上传阿里云，AI识别画面物体
3. APP通过MAVLink UDP 14550端口下发遥控指令、切换飞行模式
4. 飞控实时回传姿态、高度、电压、电机数据至网页，APP实时解析展示

## 3.2 试飞完整操作闭环
1. 硬件接线，烧录Flix飞控固件，上电校准传感器
2. 手机安装ESP32Fly APP，连接无人机WiFi
3. APP输入飞控IP，建立视频与数据通信
4. APP查看实时画面，可选开启AI目标跟踪检测
5. 遥控器SBUS输入指令，飞控四层PID解算控制电机
6. 飞行过程APP自动截图保存，落地后可查看/导出日志、截图素材

## 3.3 配套优势总结
1. 飞控端：专业级四旋翼PID飞控，双姿态解算、多重故障保护、MAVLink标准协议，兼容专业地面站
2. 移动端：独立跨平台上位机，无需电脑，随身实时图传+AI视觉识别，自动记录飞行画面
3. 一体化配套：软硬件IP、通信协议完全适配，开箱联动，无需额外协议适配修改

---
