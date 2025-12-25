#ifndef GLOBALS_H
#define GLOBALS_H
#include <Arduino.h>
#include <TinyGPS++.h>

// 全局共享对象
extern TinyGPSPlus gps;

// 系统状态标志
extern volatile bool isRaceMode;    // 是否处于竞速模式
extern volatile bool isSetupMode;   // 新增：赛道设置模式

extern volatile bool isAuthPassed;  // 蓝牙握手是否通过

// GPS 数据缓存 (用于多线程安全交换)
extern volatile double g_lat, g_lon;
extern volatile float g_spd, g_alt;
extern volatile int g_sat;
extern volatile bool g_fix;

#endif