#include <Arduino.h>
#include "System_Config.hpp"
#include "LGFX_Driver.hpp"
#include "SD_Driver.hpp"
#include "GPS_Driver.hpp"
#include "IMU_Driver.hpp"
#include "APP_UI.hpp"
#include "DataLogger.hpp"
#include "USB_Driver.hpp"

// 全局对象实例化 (保留修复)
ConfigManager sys_cfg;
LGFX tft;
GPS_Driver gps(44, 43);
String gps_log_buffer = "";

#define PIN_BAT_ADC 9

void setup()
{
  Serial.begin(115200);
  sys_cfg.load();

  pinMode(PIN_BAT_ADC, INPUT);
  tft.init();
  tft.setRotation(1);
  tft.setBrightness(128);

  initSD();
  if (sys_cfg.boot_into_usb)
  {
    // 如果标志位为真，直接运行 USB 模式逻辑
    // 这个函数里有死循环，直到用户点击屏幕重启
    run_usb_mode();
    // 理论上 run_usb_mode 不会返回，因为里面调用了 ESP.restart()
  }

  gps.begin();
  imu.begin();

  // [回滚] 移除了 setRate 调用
  // GPS 将使用模块默认的波特率和频率 (通常是 1Hz)

  init_ui();
  Serial.println("--- System Started (Standard GPS Mode) ---");
}

// ================= 任务调度区 (保持优化的结构) =================

// 任务 1: 传感器读取
void task_sensors()
{
  gps.update();
  static uint32_t t_imu = 0;
  if (millis() - t_imu > 20)
  {
    imu.update();
    t_imu = millis();
  }
}

// 任务 2: 数据记录 (10Hz)
// 即使 GPS 是 1Hz，我们也按 10Hz 记录 IMU 数据和当前的 GPS 缓存数据
// 任务 2: 数据记录与状态管理 (10Hz)
void task_logging()
{
  static uint32_t t_log = 0;
  static bool last_running_state = false;

  // --- A. 状态机：检测 开始/停止 边缘 ---
  if (sys_cfg.is_running != last_running_state)
  {
    if (sys_cfg.is_running)
    {
      // [新增] 刚开始运行 -> 记录当前时间戳，作为计时起点
      sys_cfg.session_start_ms = millis();

      // 开始录制
      if (!logger.start())
      {
        sys_cfg.is_running = false;
        Serial.println("Error: Start logging failed");
      }
    }
    else
    {
      // 刚停止 -> 保存文件
      logger.stop();
    }
    last_running_state = sys_cfg.is_running;
  }

  // --- B. 周期性记录 (100ms) ---
  if (millis() - t_log >= 100)
  {
    t_log = millis();
    if (sys_cfg.is_running)
    {
      logger.log();
    }
  }
}

// 任务 3: UI 引擎
void task_ui_engine()
{
  static uint32_t t_ui = 0;
  if (millis() - t_ui > 5)
  {
    t_ui = millis();
    update_ui_loop();
  }
}

// 任务 4: UI 数值慢刷
void task_ui_refresh()
{
  static uint32_t t_refresh = 0;
  if (millis() - t_refresh > 200)
  {
    t_refresh = millis();
    ui_refresh_data();
  }
}

void loop()
{
  task_sensors();
  task_logging();
  task_ui_engine();
  task_ui_refresh();
}