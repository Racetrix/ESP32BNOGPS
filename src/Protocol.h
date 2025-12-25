#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <Arduino.h>

// 包头定义
const uint8_t HEAD_RACE = 0xAA; // 竞速包头
const uint8_t HEAD_HB = 0xBB;   // 心跳包头 (Heartbeat)
const uint8_t TAIL = 0x55;

// 1. 竞速模式数据包 (高速, 25Hz)
struct __attribute__((packed)) RacePacket
{
    uint8_t head = HEAD_RACE;
    uint8_t fix;         // 1=Fix, 0=No
    uint8_t sats;        // 卫星数
    double lat;          // 纬度
    double lon;          // 经度
    float speed;         // 速度
    float alt;           // 海拔
    uint8_t tail = TAIL; // 简单校验/尾部
};

// 2. 心跳/待机数据包 (低速, 1Hz)
struct __attribute__((packed)) HeartbeatPacket
{
    uint8_t head = HEAD_HB;
    uint8_t sys_status; // 系统状态 (0=Idle, 1=Race, 2=WifiOn)
    uint8_t fix;
    uint8_t sats;
    float vbat; // 电池电压 (预留)
    uint8_t tail = TAIL;
};

#endif