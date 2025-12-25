# 🏎️ ESP32 High-Performance Race GPS

基于 ESP32 的高性能蓝牙 GPS 接收器，专为赛道日 (Track Day) 和跑山 (Touge) 计时设计。

本项目利用 ESP32 的 **双核特性**，将 GPS 解析与蓝牙传输分离，实现了 **10Hz** 的稳定高频数据输出，是 RaceBox 或 QStarz 等昂贵设备的 DIY 平替方案。

## ✨ 主要功能

* **⚡ 10Hz 高频刷新**：每秒 10 次位置更新，精准捕捉高速弯道轨迹（需 GPS 模块支持）。
* **🚀 双核架构**：
* `Core 0`: 专用 GPS NMEA 解析，确保数据零延迟、不丢包。
* `Core 1`: 蓝牙通信与业务逻辑，保证连接稳定。


* **📱 蓝牙透传**：通过 Bluetooth Classic (经典蓝牙) 连接手机，兼容性极佳。
* **🔋 低功耗待机**：支持“待机模式 (1Hz)”和“竞速模式 (10Hz)”动态切换。
* **🔒 安全鉴权**：简单的连接密码验证机制，防止他人误连干扰。

## 🛠️ 硬件要求

| 组件 | 推荐型号 | 说明 |
| --- | --- | --- |
| **主控板** | ESP32 DevKit V1 | 或其他 ESP32-WROOM-32 开发板 |
| **GPS 模块** | U-blox M8N / M10 / ATGM336H | **必须支持 10Hz 输出**。波特率需配置为 `115200` |
| **电源** | 3.7V 锂电池 | 连接到 ESP32 的 3.3V 或 5V (视模块而定) |

### 引脚连接 (默认)

* **GPS TX** -> **ESP32 GPIO 4**
* **GPS RX** -> **ESP32 GPIO 5**
* *(可在 `GpsTask.cpp` 中修改 `GPS_RX` 和 `GPS_TX`)*

## 📦 快速开始

1. **环境准备**：安装 [PlatformIO](https://platformio.org/) 或 Arduino IDE。
2. **依赖库**：
* `TinyGPSPlus` (by Mikal Hart)
* `BluetoothSerial` (ESP32 内置)


3. **配置 GPS**：
* 确保你的 GPS 模块已通过 u-center 或指令配置为 **10Hz** 输出频率。
* 确保 GPS 模块波特率为 **115200**。


4. **烧录**：将固件烧录至 ESP32。
5. **连接**：
* 打开手机蓝牙，搜索设备 **`ESP32_Race_Master`** 并配对。
* 打开串口助手或计时 App 进行测试。



## 📡 通信协议

### 1. 控制指令 (发送给 ESP32)

所有指令以换行符 `\n` 结尾。

| 指令 | 描述 |
| --- | --- |
| `KEY:1234` | **鉴权 (必须第一步发送)**。密码默认为 `1234`。 |
| `CMD:RACE_ON` | **开启竞速模式**。数据频率切换至 10Hz。 |
| `CMD:RACE_OFF` | **关闭竞速模式**。数据频率降至 1Hz (省电)。 |

### 2. 数据输出 (ESP32 发送给手机)

**竞速数据包 (`$RC`)** - 10Hz
格式：CSV (逗号分隔)

```text
$RC,[Fix],[Sats],[Lat],[Lon],[Speed],[Alt]

```

* `Fix`: 1=已定位, 0=未定位
* `Sats`: 卫星数量
* `Lat`: 纬度 (double)
* `Lon`: 经度 (double)
* `Speed`: 速度 (km/h)
* `Alt`: 海拔 (m)

**心跳包 (`$HB`)** - 1Hz (待机时)

```text
$HB,[Status],[Fix],[Sats],[Voltage]
```

## 📁 项目结构

* `src/main.cpp`: 主循环与调度。
* `src/GpsTask.cpp`: **Core 0** 上的 GPS 独立解析任务。
* `src/BtManager.cpp`: 蓝牙状态机、鉴权与数据发送。
* `src/Globals.h`: 跨线程共享的全局变量。

## ⚠️ 注意事项

* **互斥锁**：代码未完全实现严格的 Double 变量互斥锁保护。在极端巧合下可能出现坐标末位微小漂移，但在 10Hz 采样下通常可忽略。
* **GPS 初始化**：本项目假设 GPS 模块已经硬件配置好 10Hz。代码中虽预留了初始化指令，但针对不同模块（U-blox vs ATGM336H）指令不同，请根据实际硬件调整。

---

*Happy Racing! 🏁*