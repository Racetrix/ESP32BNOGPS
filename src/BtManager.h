#ifndef BT_MANAGER_H
#define BT_MANAGER_H

#include <Arduino.h>

// 初始化蓝牙串口
void initBluetooth();

// 处理蓝牙输入 (核心逻辑：状态守护 + 指令解析)
// 必须在 loop() 中频繁调用
void loopBluetoothInput();

// 发送高速竞速包 (由定时器调用)
void sendRacePacket();

// 发送低速心跳包 (由定时器调用)
void sendHeartbeatPacket();

void sendSetupPacket();

#endif