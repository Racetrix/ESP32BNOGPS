#ifndef GPS_TASK_H
#define GPS_TASK_H

#include <Arduino.h>

// 初始化并启动 GPS 核心任务 (运行在 Core 0)
// 包含串口初始化和后台解析逻辑
void initGpsTask();

#endif