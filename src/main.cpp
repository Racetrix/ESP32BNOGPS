#include <Arduino.h>
#include "System_Config.hpp"
#include "LGFX_Driver.hpp"
#include "SD_Driver.hpp"
#include "GPS_Driver.hpp"
#include "IMU_Driver.hpp"
#include "APP_UI.hpp"
#include "DataLogger.hpp"
#include "USB_Driver.hpp"
#include "Audio_Driver.hpp"
#include "BLE_Driver.hpp"

// 全局对象实例化
ConfigManager sys_cfg;
LGFX tft;
GPS_Driver gps(44, 43);
String gps_log_buffer = "";

#define PIN_BAT_ADC 9

void setup()
{
  Serial.begin(115200);

  // 1. 加载配置 (必须最先执行，因为 offset 值保存在 flash 里)
  sys_cfg.load();
  

  pinMode(PIN_BAT_ADC, INPUT);
  tft.init();
  tft.setRotation(1);
  tft.setBrightness(128);

  initSD();
  if (sys_cfg.boot_into_usb)
  {
    run_usb_mode();
  }

  ble.init("ESP32_Master");

  // 3. 初始化音频
  audioDriver.begin();

  // 4. 应用保存的音量
  audioDriver.setVolume(sys_cfg.volume);

  audioDriver.play("/bootup.mp3");

  gps.begin();

  // 5. 初始化 IMU 并注入校准值
  imu.begin(); // 注意：之前驱动里写的是 init()，如果你的驱动里是 begin() 请改回 begin()

  // [关键步骤] 注入“一键调平”的偏移量
  // 只要加了这一行，每次开机都会自动把球归位！
  imu.setOffsets(sys_cfg.offset_x, sys_cfg.offset_y, sys_cfg.offset_z);
  Serial.printf("IMU Offsets Applied: X=%.2f, Y=%.2f\n", sys_cfg.offset_x, sys_cfg.offset_y);

  init_ui();
  Serial.println("--- System Started ---");
}

// ================= 任务调度区 =================

// 任务 1: 传感器读取
void task_sensors()
{
  gps.update();
  static uint32_t t_imu = 0;
  if (millis() - t_imu > 20)
  {
    // 这里调用 update() 时，驱动内部会自动减去刚才设置的 offset
    imu.update();
    t_imu = millis();
  }
}

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
      // 记录开始时间
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
      // 停止录制
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
  // 音频任务 (如果未开启多线程模式)
  // 如果你在 Audio_Driver 里开了多线程，这里可以是空的
  // audioDriver.loop();

  task_sensors();
  task_logging();
  task_ui_engine();
  task_ui_refresh();
}