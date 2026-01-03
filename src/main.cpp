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
#include "CMD_Parser.hpp"
#include "Track_Manager.hpp"
TrackManager trackMgr;
// 当赛道管理器检测到起跑时调用
void handleRaceStart()
{
  Serial.println("[MAIN] Auto-Start Logging Triggered by GPS!");
  // 修改全局状态，task_logging 里的状态机会在下一帧自动调用 logger.start()
  sys_cfg.is_running = true;

  // 你甚至可以在这里加个蜂鸣器提示音
  // audioDriver.playBeep();
}

// 当赛道管理器检测到完赛时调用 (仅 Sprint 模式)
void handleRaceFinish()
{
  Serial.println("[MAIN] Auto-Stop Logging Triggered by GPS!");
  sys_cfg.is_running = false;
}

void onBleDataReceived(String data)
{
  // 收到蓝牙数据 -> 转给解析器
  cmdParser.parse(data);
}

// 全局对象实例化
ConfigManager sys_cfg;
LGFX tft;
GPS_Driver gps(44, 43);
IMU_Driver imu(14, 21);
BLE_Driver ble;
extern Audio_Driver audioDriver;
String gps_log_buffer = "";
// extern bool sd_connected;
bool sd_connected = false;
// DataLogger logger; // 已经在 DataLogger.hpp 实例化

#define PIN_BAT_ADC 9

void setup()
{
  Serial.begin(115200);
  uint32_t wait_start = millis();
  while (!Serial && (millis() - wait_start < 2000))
  {
    delay(10); // 等待电脑连接...
  }

  Serial.println("--- Booting ---");

  // 1. 加载配置 (必须最先执行)
  sys_cfg.load();

  // pinMode(PIN_BAT_ADC, INPUT);

  tft.init();
  tft.setRotation(1);
  tft.setBrightness(128);

  initSD();
  if (sys_cfg.boot_into_usb)
  {
    run_usb_mode();
  }

  // 3. 初始化 BLE
  ble.init("RaceDash_Telemetry", onBleDataReceived);

  // 4. 初始化音频
  audioDriver.begin();

  delay(200);
  audioDriver.setVolume(sys_cfg.volume);

  audioDriver.play("/BootUP.wav");

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
  trackMgr.attachOnStart(handleRaceStart);
  trackMgr.attachOnFinish(handleRaceFinish);
  init_ui();

  Serial.println("--- System Started Successfully ---");
}

// ================= 任务调度区 =================

void task_sensors()
{
  // 1. 更新 GPS 驱动获取最新数据
  gps.update();

  // 2. [核心修复] 将 GPS 数据喂给赛道管理器！！！
  // 如果没有这一行，trackMgr 永远不知道你现在的坐标，也就永远不会触发 Start
  if (gps.tgps.location.isValid())
  {
    trackMgr.update(
        gps.tgps.location.lat(),
        gps.tgps.location.lng(),
        millis());
  }

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
    // [修改] 只有当蓝牙不忙的时候，才尝试发送心跳
    if (!ble.isTxBusy)
    {
      cmdParser.sendTelemetry();
    }
    if (sys_cfg.is_running && sd_connected)
    {
      logger.log();
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

// main.cpp

void task_ui_refresh()
{
  static uint32_t t_refresh = 0;
  if (millis() - t_refresh > 200)
  {
    t_refresh = millis();

    // [优化] 只有当前是主屏幕时，才运行常规的数据刷新
    // 零百界面有自己的高速定时器，不需要这个低速的干扰
    if (lv_scr_act() == ui_ScreenMain)
    {
      ui_refresh_data();
    }
  }
}
void loop()
{
  task_sensors();
  task_logging();
  task_ui_engine();
  task_ui_refresh();
}