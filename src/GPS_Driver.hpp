#pragma once
#include <Arduino.h>
#include <TinyGPS++.h>
#include "BLE_Driver.hpp"
#include "GPSAutoBaud.hpp"
// #include "System_Config.hpp"
// 定义全局日志缓冲区
extern String gps_log_buffer;
// extern bool gps_10hz_mode;
extern BLE_Driver ble;

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
                // 1. 实例化自动检测器
                GPSAutoBaud autobaud(serial, rxPin, txPin);

                // // 2. 执行检测
                uint32_t detectedBaud = autobaud.detect();
                 Serial.printf("Detected GPS baud rate: %lu\n", detectedBaud);
                 delay(100);
                 // 保持 115200，因为你的 GPS 模块可能已经配置过，或者默认就是这个
                 serial->begin(detectedBaud, SERIAL_8N1, rxPin, txPin);
                 _bootTime = millis();
                 _isConfigured = false;
        // serial->begin(9600, SERIAL_8N1, rxPin, txPin);
        // delay(100);
        // // 配置波特率 115200 的 UBX 指令
        // uint8_t setBaud[] = {
        //     0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00,
        //     0xD0, 0x08, 0x00, 0x00, 0x00, 0xC2, 0x01, 0x00,
        //     0x07, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00};
        // sendUBX(setBaud, sizeof(setBaud));
        // delay(200);
        // serial->end(); // 关闭 9600

        // // 2. 以 115200 重连
        // serial->begin(115200, SERIAL_8N1, rxPin, txPin);
        // delay(100);

        // // 3. 设置刷新率为 8Hz (125ms)
        // // 0x007D = 125ms
        // uint8_t setRate[] = {
        //     0x06, 0x08, 0x06, 0x00, 0xC8, 0x00, 0x01, 0x00, 0x01, 0x00};
        // sendUBX(setRate, sizeof(setRate));

        // Serial.println(">>> 配置指令已发送，准备监测刷新率...");
    }

    void update()
    {
        while (serial->available())
        {
            // 1. 读取原始字节
            char c = serial->read();
            // Serial.write(c);

            // 2. [原有逻辑] 喂给 TinyGPS++ 解析 (给屏幕UI和算法用)
            tgps.encode(c);

            // 3. [原有逻辑] 日志缓冲
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

        // 配置逻辑保持不变
        if (sys_cfg.gps_10hz_mode && !_isConfigured && (millis() - _bootTime > 3000))
        {
            setUblox10Hz();
            // setUbloxRate8Hz();
            // setUbloxBaud115200();
            delay(100);
            _isConfigured = true;
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
    void sendUBX(const uint8_t *payload, uint8_t len)
    {
        uint8_t CK_A = 0, CK_B = 0;
        serial->write(0xB5);
        serial->write(0x62);
        for (int i = 0; i < len; i++)
        {
            CK_A = CK_A + payload[i];
            CK_B = CK_B + CK_A;
            serial->write(payload[i]);
        }
        serial->write(CK_A);
        serial->write(CK_B);
        serial->println();
    }
    // 将 U-blox 7代 GPS 波特率修改为 115200
    void setUbloxBaud115200()
    {
        // UBX-CFG-PRT 指令 (Port Configuration)
        // 针对 UART1 (PortID=1) 设置为 115200, 8N1
        uint8_t packet[] = {
            0xB5, 0x62, // 头部 (Header)
            0x06, 0x00, // Class ID (UBX-CFG-PRT)
            0x14, 0x00, // 长度 (20 Bytes)

            // Payload (有效载荷)
            0x01,                   // Port ID: 1 (UART1)
            0x00,                   // Reserved
            0x00, 0x00,             // TX Ready
            0xD0, 0x08, 0x00, 0x00, // Mode: 8N1 (0x000008D0)
            0x00, 0xC2, 0x01, 0x00, // BaudRate: 115200 (0x0001C200) Little Endian
            0x07, 0x00,             // In Proto Mask: UBX+NMEA+RTCM
            0x03, 0x00,             // Out Proto Mask: UBX+NMEA
            0x00, 0x00,             // Flags
            0x00, 0x00,             // Reserved

            // 校验和 (针对上述特定 Payload 计算得出)
            0x5C, 0x06 // CK_A, CK_B
        };

        // 发送指令
        serial->write(packet, sizeof(packet));

        // ⚠️ 关键步骤：发送完这个指令后，GPS 会切换波特率
        // 您的 ESP32 也必须紧接着切换到 115200 才能继续通信
        delay(200); // 等待 GPS 处理
    }
    // 将 U-blox 7代 GPS 刷新率修改为 8Hz (125ms)
    void setUbloxRate8Hz()
    {
        // UBX-CFG-RATE 指令
        // 1000ms / 8 = 125ms = 0x007D
        uint8_t packet[] = {
            0xB5, 0x62, // 头部
            0x06, 0x08, // Class ID (UBX-CFG-RATE)
            0x06, 0x00, // 长度 (6 Bytes)

            // Payload
            0x7D, 0x00, // measRate: 125ms (0x007D) -> 8Hz
            0x01, 0x00, // navRate: 1 (每个测量周期都输出导航解)
            0x01, 0x00, // timeRef: 1 (GPS Time)

            // 校验和 (针对 8Hz 计算得出)
            0x93, 0xC8 // CK_A, CK_B
        };

        serial->write(packet, sizeof(packet));
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