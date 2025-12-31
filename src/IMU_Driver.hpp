#pragma once
#include <Arduino.h>
// 必须引用配置，才能知道要不要反转
#include "System_Config.hpp"

class IMU_Driver
{
private:
    HardwareSerial *serial;
    uint8_t rxPin, txPin;

    const uint8_t START_BYTE = 0xAA;
    const uint8_t WRITE_CMD = 0x00;
    const uint8_t READ_CMD = 0x01;
    const uint8_t REG_DATA_START = 0x1A;
    const uint8_t DATA_LEN = 20;
    const uint8_t REG_OPR_MODE = 0x3D;
    const uint8_t OPR_MODE_NDOF = 0x0C;

    const float ALPHA = 0.15;

    float _off_head = 0, _off_roll = 0, _off_pit = 0;
    float _off_lon = 0, _off_lat = 0;

public:
    float heading = 0.0;
    float roll = 0.0;
    float pitch = 0.0;
    float lat_g = 0.0;
    float lon_g = 0.0;

    // 原始数据缓存
    float raw_head = 0, raw_roll = 0, raw_pit = 0;
    float raw_lon = 0, raw_lat = 0;

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

    void setAllOffsets(float head, float roll, float pit, float lon, float lat)
    {
        _off_head = head;
        _off_roll = roll;
        _off_pit = pit;
        _off_lon = lon;
        _off_lat = lat;
    }

    void getRawValues(float &h, float &r, float &p, float &lon, float &lat)
    {
        h = raw_head;
        r = raw_roll;
        p = raw_pit;
        lon = raw_lon;
        lat = raw_lat;
    }

    void update()
    {
        if (serial->available() >= (2 + DATA_LEN))
        {
            if (serial->peek() != 0xBB)
            {
                serial->read();
                return;
            }

            uint8_t header[2];
            serial->readBytes(header, 2);

            if (header[0] == 0xBB && header[1] == DATA_LEN)
            {
                uint8_t buf[DATA_LEN];
                serial->readBytes(buf, DATA_LEN);

                // 1. 解析原始欧拉角
                int16_t h_int = (int16_t)((buf[1] << 8) | buf[0]);
                int16_t r_int = (int16_t)((buf[3] << 8) | buf[2]);
                int16_t p_int = (int16_t)((buf[5] << 8) | buf[4]);

                raw_head = h_int / 16.0;
                raw_roll = r_int / 16.0;
                raw_pit = p_int / 16.0;

                // 2. 解析原始加速度
                int16_t x_int = (int16_t)((buf[15] << 8) | buf[14]);
                int16_t y_int = (int16_t)((buf[17] << 8) | buf[16]);

                float acc_x = (x_int / 100.0) / 9.81;
                float acc_y = (y_int / 100.0) / 9.81;

                // --- [新增] 处理轴向交换 (Swap) ---
                // 先交换，再赋值给 raw
                if (sys_cfg.imu_swap_axis)
                {
                    raw_lat = acc_y; // 交换后 Y 变成横向
                    raw_lon = acc_x; // 交换后 X 变成纵向
                }
                else
                {
                    raw_lat = acc_x;
                    raw_lon = acc_y;
                }

                // --- [新增] 处理反转 (Invert) ---
                // 注意：这里是对 raw 数据反转，之后再减 offset
                if (sys_cfg.imu_invert_x)
                    raw_lat = -raw_lat;
                if (sys_cfg.imu_invert_y)
                    raw_lon = -raw_lon;

                // 3. 应用偏移量
                roll = raw_roll - _off_roll;
                pitch = raw_pit - _off_pit;

                heading = raw_head - _off_head;
                while (heading < 0)
                    heading += 360.0;
                while (heading >= 360)
                    heading -= 360.0;

                float calibrated_lat = raw_lat - _off_lat;
                float calibrated_lon = raw_lon - _off_lon;

                // 4. 滤波
                lat_g = (lat_g * (1.0 - ALPHA)) + (calibrated_lat * ALPHA);
                lon_g = (lon_g * (1.0 - ALPHA)) + (calibrated_lon * ALPHA);

                isConnected = true;
            }
        }

        static uint32_t last_req = 0;
        if (millis() - last_req > 20)
        {
            if (serial->available() > 64)
            {
                while (serial->available())
                    serial->read();
            }
            sendCommand(READ_CMD, REG_DATA_START, DATA_LEN, NULL);
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

extern IMU_Driver imu;