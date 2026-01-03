#pragma once
#include <Arduino.h>
#include <TinyGPS++.h>
// #include "System_Config.hpp"
// 定义全局日志缓冲区
extern String gps_log_buffer;
// extern bool gps_10hz_mode;

class GPS_Driver
{
private:
    HardwareSerial *serial;
    uint8_t rxPin, txPin;
    const size_t MAX_LOG_SIZE = 1024;
    bool _isConfigured = false; // [新增] 记录是否已配置
    uint32_t _bootTime = 0;

public:
    TinyGPSPlus tgps;

    GPS_Driver(uint8_t rx, uint8_t tx) : rxPin(rx), txPin(tx)
    {
        serial = &Serial1;
    }

    void begin()
    {
        // 保持 115200，因为你的 GPS 模块可能已经配置过，或者默认就是这个
        serial->begin(115200, SERIAL_8N1, rxPin, txPin);
        _bootTime = millis();
        _isConfigured = false;
        // if (sys_cfg.gps_10hz_mode)
        // {
        //     sleep(20);
        //     setUblox10Hz();
        // }
    }

    void update()
    {
        while (serial->available())
        {
            char c = serial->read();
            tgps.encode(c);

            // 日志缓冲逻辑
            if (gps_log_buffer.length() < MAX_LOG_SIZE)
            {
                gps_log_buffer += c;
            }
            else
            {
                gps_log_buffer = gps_log_buffer.substring(MAX_LOG_SIZE / 2);
                gps_log_buffer += c;
            }
        }
        if (sys_cfg.gps_10hz_mode && !_isConfigured && (millis() - _bootTime > 3000))
        {
            setUblox10Hz(); // 发送指令
            _isConfigured = true;   // 标记已完成，以后不再发
        }
    }

    float getSpeed()
    {
        return tgps.speed.isValid() ? tgps.speed.kmph() : 0.0;
    }

    int getSatellites()
    {
        return tgps.satellites.isValid() ? tgps.satellites.value() : 0;
    }
    // 发送 U-blox 10Hz 配置指令 (UBX-CFG-RATE)
    void setUblox10Hz()
    {
        // 指令含义: 100ms 测量周期 (=10Hz), 1次导航周期, GPS时间对齐
        uint8_t packet[] = {
            0xB5, 0x62, // 头部 (Header)
            0x06, 0x08, // Class ID (UBX-CFG-RATE)
            0x06, 0x00, // 长度 (6 Bytes)
            0x64, 0x00, // measRate: 100ms (0x0064) -> 10Hz
            0x01, 0x00, // navRate: 1
            0x01, 0x00, // timeRef: 1 (GPS Time)
            0x7A, 0x12  // 校验和 (CK_A, CK_B)
        };

        // 发送二进制数据
        serial->write(packet, sizeof(packet));

        // 建议等待一小会儿让模块处理
        delay(100);
    }
};

// 声明外部对象 (main.cpp 中实例化)
extern GPS_Driver gps;