#include <Arduino.h>
#include "Globals.h"
#include "Protocol.h"
#include "GpsTask.h"
#include "BtManager.h"
// #include "WifiManager.h" // <--- 已删除

// 定时器变量
unsigned long lastMsgTime = 0;
const int RACE_INTERVAL = 100;  // 10Hz
const int IDLE_INTERVAL = 1000; // 1Hz

void setup()
{
  Serial.begin(115200);

  initGpsTask();   // 启动 GPS 核心
  initBluetooth(); // 启动蓝牙
  // initWiFi();   // <--- 已删除

  Serial.println("System Ready. Waiting for Bluetooth...");
}

void loop()
{
  // 1. 处理输入
  loopBluetoothInput();
  // loopWiFi(); // <--- 已删除

  // 2. 发送数据逻辑
  unsigned long now = millis();
  int interval = isRaceMode ? RACE_INTERVAL : IDLE_INTERVAL;

  if (now - lastMsgTime > interval)
  {
    lastMsgTime = now;

    if (isRaceMode)
    {
      sendRacePacket();
    }
    else
    {
      sendHeartbeatPacket();
      // sendJsonToWeb(); // <--- 已删除
    }
  }
}