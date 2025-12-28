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

    // 增加一个状态标记，放在 class private 里，或者 update 里的 static
    // 为了简单，我们直接修改 update 函数逻辑

    void update()
    {
        // 1. 如果缓冲区有足够的数据，直接读取 (非阻塞)
        if (serial->available() >= 8) // 只有数据够了才读
        {
            // 检查帧头
            if (serial->peek() != 0xBB) // 如果第一个字节不是 BB，说明数据错位了
            {
                serial->read(); // 丢弃一个字节，尝试重新对齐
                return;
            }

            // 再次确认协议头 (防止偶然的0xBB)
            // 这里我们先读两个字节看看
            uint8_t header[2];
            serial->readBytes(header, 2);

            if (header[0] == 0xBB && header[1] == 6)
            {
                uint8_t buf[6];
                serial->readBytes(buf, 6);

                int16_t x_raw_int = (int16_t)((buf[1] << 8) | buf[0]);
                int16_t y_raw_int = (int16_t)((buf[3] << 8) | buf[2]);
                int16_t z_raw_int = (int16_t)((buf[5] << 8) | buf[4]);

                ax_raw = (x_raw_int / 100.0) / 9.81;
                ay_raw = (y_raw_int / 100.0) / 9.81;
                az_raw = (z_raw_int / 100.0) / 9.81;

                ax = (ax_raw * ALPHA) + (ax * (1.0 - ALPHA));
                ay = (ay_raw * ALPHA) + (ay * (1.0 - ALPHA));
                az = (az_raw * ALPHA) + (az * (1.0 - ALPHA));

                isConnected = true;
            }
        }

        // 2. 发送请求 (不需要每次循环都发，控制频率)
        static uint32_t last_req = 0;
        if (millis() - last_req > 20) // 每 20ms 发一次请求
        {
            // 在发送前，不要清空 buffer！因为可能上一帧的数据刚到
            // 只在 buffer 溢出或者出错时清空
            if (serial->available() > 64)
            {
                while (serial->available())
                    serial->read();
            }

            sendCommand(READ_CMD, REG_LIA_DATA, 6, NULL);
            last_req = millis();
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