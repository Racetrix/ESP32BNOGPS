#pragma once
#include <Arduino.h>
#include <TinyGPS++.h>

// 定义全局日志缓冲区
extern String gps_log_buffer;

class GPS_Driver
{
private:
    HardwareSerial *serial;
    uint8_t rxPin, txPin;
    const size_t MAX_LOG_SIZE = 1024;

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
    }

    float getSpeed()
    {
        return tgps.speed.isValid() ? tgps.speed.kmph() : 0.0;
    }

    int getSatellites()
    {
        return tgps.satellites.isValid() ? tgps.satellites.value() : 0;
    }
};

// 声明外部对象 (main.cpp 中实例化)
extern GPS_Driver gps;