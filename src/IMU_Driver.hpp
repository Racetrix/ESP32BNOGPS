#pragma once
#include <Arduino.h>

class IMU_Driver
{
private:
    HardwareSerial *serial;
    uint8_t rxPin, txPin;

    const uint8_t START_BYTE = 0xAA;
    const uint8_t WRITE_CMD = 0x00;
    const uint8_t READ_CMD = 0x01;
    const uint8_t REG_OPR_MODE = 0x3D;
    const uint8_t REG_LIA_DATA = 0x28;
    const uint8_t OPR_MODE_NDOF = 0x0C;

    // 滤波系数 (用于 UI 显示)
    const float ALPHA = 0.15;

public:
    // --- 给 UI 用的平滑数据 (Smoothed) ---
    float ax = 0.0;
    float ay = 0.0;
    float az = 0.0;

    // --- [新增] 给 SD 卡用的原始数据 (Raw) ---
    float ax_raw = 0.0;
    float ay_raw = 0.0;
    float az_raw = 0.0;

    bool isConnected = false;

    IMU_Driver(uint8_t rx, uint8_t tx) : rxPin(rx), txPin(tx)
    {
        serial = &Serial2;
    }

    void begin()
    {
        serial->begin(115200, SERIAL_8N1, rxPin, txPin);
        serial->setTimeout(5);
        delay(100);
        uint8_t modeData = OPR_MODE_NDOF;
        sendCommand(WRITE_CMD, REG_OPR_MODE, 1, &modeData);
        delay(600);
        while (serial->available())
            serial->read();
        isConnected = true;
    }

    void update()
    {
        while (serial->available())
            serial->read(); // 清空缓存
        sendCommand(READ_CMD, REG_LIA_DATA, 6, NULL);

        unsigned long start = millis();
        while (serial->available() < 8)
        {
            if (millis() - start > 5)
                return;
        }

        if (serial->read() == 0xBB)
        {
            if (serial->read() == 6)
            {
                uint8_t buf[6];
                serial->readBytes(buf, 6);

                int16_t x_raw_int = (int16_t)((buf[1] << 8) | buf[0]);
                int16_t y_raw_int = (int16_t)((buf[3] << 8) | buf[2]);
                int16_t z_raw_int = (int16_t)((buf[5] << 8) | buf[4]);

                // 1. 计算原始物理值 (Raw) - 直接保存，不滤波！
                ax_raw = (x_raw_int / 100.0) / 9.81;
                ay_raw = (y_raw_int / 100.0) / 9.81;
                az_raw = (z_raw_int / 100.0) / 9.81;

                // 2. 计算平滑值 (Smoothed) - 仅供 UI 使用
                ax = (ax_raw * ALPHA) + (ax * (1.0 - ALPHA));
                ay = (ay_raw * ALPHA) + (ay * (1.0 - ALPHA));
                az = (az_raw * ALPHA) + (az * (1.0 - ALPHA));

                isConnected = true;
            }
        }
    }

private:
    void sendCommand(uint8_t rw, uint8_t reg, uint8_t len, uint8_t *data)
    {
        serial->write(START_BYTE);
        serial->write(rw);
        serial->write(reg);
        serial->write(len);
        if (rw == WRITE_CMD && data != NULL)
        {
            for (uint8_t i = 0; i < len; i++)
                serial->write(data[i]);
        }
    }
};

IMU_Driver imu(14, 21);