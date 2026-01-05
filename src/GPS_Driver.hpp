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
            setUbloxGPSBeiDou();
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
    void setUbloxGPSBeiDou()
    {
        // 指令含义: 配置 GNSS 星座 (UBX-CFG-GNSS)
        // 我们将显式配置 5 个星座块: GPS, SBAS, BeiDou, QZSS, GLONASS
        // 策略: 开启 GPS+北斗+SBAS，关闭 GLONASS+QZSS (为了在 10Hz 下节省芯片算力)

        uint8_t packet[] = {
            0xB5, 0x62, // 头部 (Header)
            0x06, 0x3E, // Class ID (UBX-CFG-GNSS)
            0x2C, 0x00, // 长度: 44 Bytes (Header 4 + 5个Block*8)

            // --- Payload 开始 ---
            0x00, 0x00, 0x20, 0x05, // msgVer=0, trkCh=32, count=5 (配置5个块)

            // Block 1: GPS (ID 0) -> 开启
            0x00, 0x08, 0x10, 0x00, 0x01, 0x00, 0x01, 0x01,

            // Block 2: SBAS (ID 1) -> 开启 (增强定位)
            0x01, 0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0x01,

            // Block 3: BeiDou (ID 3) -> 开启 !!!
            0x03, 0x08, 0x10, 0x00, 0x01, 0x00, 0x01, 0x01,

            // Block 4: QZSS (ID 5) -> 关闭 (日本区域，国内没用，关掉省电)
            0x05, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x01,

            // Block 5: GLONASS (ID 6) -> 关闭 (为了给10Hz腾出算力，建议关掉)
            0x06, 0x08, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x01,
            // --- Payload 结束 ---

            0x00, 0x00 // 预留位置给校验和 (CK_A, CK_B)
        };

        // 自动计算校验和 (算法: RFC 1145 / UBX Standard)
        // 从 Class (0x06) 开始算，不包含头部 0xB5 0x62
        uint8_t ck_a = 0, ck_b = 0;
        for (int i = 2; i < sizeof(packet) - 2; i++)
        {
            ck_a += packet[i];
            ck_b += ck_a;
        }

        // 填入校验和
        packet[sizeof(packet) - 2] = ck_a;
        packet[sizeof(packet) - 1] = ck_b;

        // 发送二进制数据
        serial->write(packet, sizeof(packet));

        // 这个指令比较重，建议多给点时间处理
        delay(200);
    }
};

// 声明外部对象 (main.cpp 中实例化)
extern GPS_Driver gps;