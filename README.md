# ESP32 Mini Drone - RC QGC Photo WiFi

基于 ESP32S3-EYE 的迷你无人机飞行控制系统，支持 RC 遥控、QGC 地面站、Wi-Fi 实时监控和摄像头拍照功能。

## ✨ 功能特性

### 飞行控制
- **级联 PID 控制**：角度外环 → 角速度内环 → 电机输出
- **四元数姿态估计**：陀螺仪积分 + 加速度计修正，避免万向节锁死
- **多种飞行模式**：RAW（直驱）、ACRO（特技）、STAB（自稳）、AUTO（自动）、OBSTACLE（避障）
- **故障安全保护**：RC 信号丢失自动降落、自动飞行模式手动接管

### 传感器
- **MPU5600 IMU**：6轴陀螺仪/加速度计，支持自动校准
- **PM280 气压计**：高度测量，量程 300~1100 hPa
- **VL53L0X 距离传感器**：TOF 测距，量程 30~2000 mm
- **电压监测**：电机电源和芯片供电电压

### 通信
- **SBUS RC 接收**：支持自动通道校准
- **Wi-Fi STA 模式**：连接路由器，支持静态 IP 配置
- **MAVLink 协议**：与 QGC 地面站通信，支持参数管理、姿态显示、解锁/上锁
- **Web 实时监控**：实时摄像头画面、传感器数据、电机状态

### 摄像头
- **OV3660 摄像头**：8位并行接口，支持 JPEG 图像捕获
- **QQVGA 分辨率**：160×120，支持实时预览

## 🛠️ 硬件要求

| 组件 | 型号 | 说明 |
|------|------|------|
| 主控芯片 | ESP32S3-EYE | 集成 OV3660 摄像头 |
| IMU | MPU5600 | I2C 接口，地址 0x68 |
| 气压计 | PM280 | I2C 接口，地址 0x76 |
| 距离传感器 | VL53L0X | I2C 接口，地址 0x29 |
| RC 接收器 | SBUS 协议 | UART2 接口，RX=14 |
| 电机驱动 | MOSFET/ESC | LEDC PWM 输出 |
| 电机 | 直流/无刷电机 | 4 个 |

### 引脚分配

| 功能 | 引脚 |
|------|------|
| 电机 0（后左） | 41 |
| 电机 1（后右） | 40 |
| 电机 2（前右） | 39 |
| 电机 3（前左） | 42 |
| SBUS RX | 14 |
| I2C SDA | 8 |
| I2C SCL | 9 |
| 电池电压 ADC | 12 |
| 芯片电压 ADC | 13 |

## 📦 软件安装

### 环境要求
- Arduino IDE 或 PlatformIO
- ESP32 Arduino 核心
- ESP32 Camera 库

### 安装步骤

1. **克隆项目**
   ```bash
   git clone https://github.com/your-repo/esp32-mini-drone.git
   cd esp32-mini-drone
   ```

2. **安装依赖库**
   - `SBUS` - RC 信号接收
   - `MAVLink` - 地面站通信
   - `Preferences` - 参数存储
   - `ESP32 Camera` - 摄像头驱动

3. **配置 Wi-Fi**
   编辑 `wifi.ino`，设置 SSID 和密码：
   ```cpp
   #define WIFI_SSID "your-ssid"
   #define WIFI_PASSWORD "your-password"
   ```

4. **上传固件**
   - 选择开发板：`ESP32S3 Dev Module`
   - 上传速度：`921600`
   - 分区方案：`Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)`

## 🚀 使用方法

### 首次启动

1. **硬件连接**：确保所有传感器和电机连接正确
2. **串口通信**：打开串口监视器，波特率 `115200`
3. **校准流程**：
   ```
   > ca    # 校准加速度计（保持无人机水平静止）
   > cr    # 校准 RC 遥控（按提示操作摇杆）
   ```

### 命令行界面

```
help    - 显示帮助
p       - 显示所有参数
p <name> <value> - 设置参数
preset  - 重置参数
arm     - 解锁无人机
disarm  - 锁定无人机
ps      - 显示姿态（欧拉角）
psq     - 显示姿态（四元数）
imu     - 显示 IMU 数据
rc      - 显示 RC 数据
mot     - 显示电机输出
sys     - 显示系统信息
time    - 显示时间信息
wifi    - 显示 Wi-Fi 信息
raw/stab/acro/auto/obstacle - 切换飞行模式
mfr/mfl/mrr/mrl - 测试单个电机
log [dump] - 查看日志
reboot  - 重启无人机
```

### Web 监控

连接无人机 Wi-Fi 后，在浏览器中访问：
```
http://........
```

监控页面包含：
- 摄像头实时画面
- 电机电源电压和芯片电压
- 气压、海拔高度、距离、温度
- 电机占空比实时显示

### QGC 地面站

1. 打开 QGroundControl
2. 添加 UDP 连接：端口 `14550`
3. 等待无人机连接（蓝色心跳图标）
4. 功能：
   - 实时姿态显示
   - 参数管理（调参）
   - 解锁/上锁
   - 飞行模式切换

## 📁 项目结构

```
.
├── RC_QGC_photo_wifi.ino    # 主入口，FreeRTOS 任务创建
├── control.ino              # 级联 PID 控制和电机混合
├── estimate.ino             # 四元数姿态估计
├── safety.ino               # 故障安全保护
├── imu.ino                  # MPU9250 IMU 驱动和校准
├── sensors.ino              # PM280 和 VL53L0X 驱动
├── voltage.ino              # ADC 电压测量
├── motors.ino               # LEDC PWM 电机驱动
├── rc.ino                   # SBUS RC 接收和校准
├── camera.ino               # OV3660 摄像头驱动
├── wifi.ino                 # Wi-Fi 和 Web 服务器
├── mavlink.ino              # MAVLink 协议通信
├── cli.ino                  # 命令行界面
├── log.ino                  # RAM 日志记录
├── led.ino                  # LED 状态指示
├── time.ino                 # 时间管理
├── parameters.ino           # 参数存储
├── vector.h                 # 3D 向量类
├── quaternion.h             # 四元数类
├── pid.h                    # PID 控制器
├── lpf.h                    # 低通滤波器
└── util.h                   # 工具函数
```

## 🎛️ 飞行模式说明

| 模式 | 说明 | 适用场景 |
|------|------|----------|
| RAW | 电机直驱，无控制 | 测试电机 |
| ACRO | 角速度控制 | 特技飞行 |
| STAB | 自稳模式，角度控制 | 日常飞行 |
| AUTO | 自动模式，地面站控制 | 自主飞行 |
| OBSTACLE | 避障模式 | 避障飞行 |

## 🔧 主要参数

### PID 参数
- `CTL_R_RATE_P/I/D` - 横滚角速度 PID
- `CTL_P_RATE_P/I/D` - 俯仰角速度 PID
- `CTL_Y_RATE_P/I/D` - 偏航角速度 PID
- `CTL_R_P/I/D` - 横滚角度 PID
- `CTL_P_P/I/D` - 俯仰角度 PID

### 限制参数
- `CTL_R_RATE_MAX` - 最大横滚角速度 (rad/s)
- `CTL_P_RATE_MAX` - 最大俯仰角速度 (rad/s)
- `CTL_Y_RATE_MAX` - 最大偏航角速度 (rad/s)
- `CTL_TILT_MAX` - 最大倾斜角度 (rad)

### 姿态估计参数
- `EST_ACC_WEIGHT` - 加速度计修正权重
- `EST_RATES_LPF_A` - 角速度低通滤波系数

## ⚠️ 安全注意事项

1. **首次飞行前**：确保所有电机方向正确，桨叶安装牢固
2. **测试电机**：使用 `mfr/mfl/mrr/mrl` 命令测试单个电机，不要装桨叶
3. **解锁条件**：油门必须低于安全阈值才能解锁
4. **飞行环境**：选择空旷无风环境，远离人群和障碍物
5. **电池电压**：监控电池电压，低电压时及时降落

