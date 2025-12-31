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
IMU_Driver imu(14, 21);
String gps_log_buffer = "";
// DataLogger logger; // 已经在 DataLogger.hpp 实例化

#define PIN_BAT_ADC 9

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println("--- Booting ---");

  // 1. 加载配置 (必须最先执行)
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

  // 3. 初始化 BLE
  ble.init("RaceDash_Telemetry");

  // 4. 初始化音频
  audioDriver.begin();
  delay(200);
  audioDriver.setVolume(sys_cfg.volume);

  // 5. 初始化 GPS
  gps.begin();

  // 6. 初始化 IMU
  imu.begin();

  // --- [修复这里] 使用新的 API 注入 5 个校准参数 ---
  imu.setAllOffsets(
      sys_cfg.offset_heading,
      sys_cfg.offset_roll,
      sys_cfg.offset_pitch,
      sys_cfg.offset_lon,
      sys_cfg.offset_lat);

  Serial.printf("IMU Offsets Applied: Lon=%.2f, Lat=%.2f\n", sys_cfg.offset_lon, sys_cfg.offset_lat);

  init_ui();

  Serial.println("--- System Started Successfully ---");
}

// ================= 任务调度区 =================

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

void task_logging()
{
  static uint32_t t_log = 0;
  static bool last_running_state = false;

  // 状态机：检测 开始/停止
  if (sys_cfg.is_running != last_running_state)
  {
    if (sys_cfg.is_running)
    {
      sys_cfg.session_start_ms = millis();
      if (!logger.start())
      {
        sys_cfg.is_running = false;
        Serial.println("Error: Start logging failed");
      }
    }
    else
    {
      logger.stop();
    }
    last_running_state = sys_cfg.is_running;
  }

  // 周期性记录 (10Hz)
  if (millis() - t_log >= 100)
  {
    t_log = millis();
    if (sys_cfg.is_running)
    {
      logger.log();

      // BLE 发送: 发送最重要的几个动态参数
      if (ble.isConnected())
      {
        // 格式: Time, Heading, LonG, LatG
        ble.printf("%lu,%.1f,%.2f,%.2f", millis(), imu.heading, imu.lon_g, imu.lat_g);
      }
    }
  }
}

void task_ui_engine()
{
  static uint32_t t_ui = 0;
  if (millis() - t_ui > 5)
  {
    t_ui = millis();
    update_ui_loop();
  }
}

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