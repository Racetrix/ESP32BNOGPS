#pragma once
#include <Arduino.h>
#include "System_Config.hpp" // 必须包含配置

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

                // 1. 读取原始数据 (暂存临时变量)
                int16_t h_int = (int16_t)((buf[1] << 8) | buf[0]);
                int16_t r_int = (int16_t)((buf[3] << 8) | buf[2]);
                int16_t p_int = (int16_t)((buf[5] << 8) | buf[4]);

                float temp_head = h_int / 16.0;
                float temp_roll = r_int / 16.0;
                float temp_pit = p_int / 16.0;

                int16_t x_int = (int16_t)((buf[15] << 8) | buf[14]);
                int16_t y_int = (int16_t)((buf[17] << 8) | buf[16]);

                float temp_ax = (x_int / 100.0) / 9.81;
                float temp_ay = (y_int / 100.0) / 9.81;

                // 2. --- [核心修复] 处理轴向交换 (Swap) ---
                // 注意：必须同时交换 G 值 和 姿态角
                if (sys_cfg.imu_swap_axis)
                {
                    // G值交换
                    raw_lat = temp_ay;
                    raw_lon = temp_ax;

                    // 姿态角交换 (Roll <-> Pitch)
                    // 注意：物理旋转90度后，Roll变成Pitch，Pitch变成-Roll (取决于旋转方向)
                    // 这里做简单交换，如果方向反了，用户可以用 Invert 修正
                    raw_roll = temp_pit;
                    raw_pit = temp_roll;
                }
                else
                {
                    raw_lat = temp_ax;
                    raw_lon = temp_ay;
                    raw_roll = temp_roll;
                    raw_pit = temp_pit;
                }

                // 航向角通常不受 Swap 影响 (它是绕 Z 轴)，除非板子竖起来装
                raw_head = temp_head;

                // 3. --- 处理反转 (Invert) ---
                // 如果 G 值反了，对应的角度通常也需要反
                if (sys_cfg.imu_invert_x)
                {
                    raw_lat = -raw_lat;
                    raw_roll = -raw_roll; // 横向 G 反了，横滚角自然也反了
                }
                if (sys_cfg.imu_invert_y)
                {
                    raw_lon = -raw_lon;
                    raw_pit = -raw_pit;
                }

                // 4. --- 应用校准偏移量 ---
                // 警告：一旦修改了 Swap/Invert，旧的偏移量就会失效
                // 必须提醒用户重新 Calibrate！
                roll = raw_roll - _off_roll;
                pitch = raw_pit - _off_pit;

                heading = raw_head - _off_head;
                while (heading < 0)
                    heading += 360.0;
                while (heading >= 360)
                    heading -= 360.0;

                float calibrated_lat = raw_lat - _off_lat;
                float calibrated_lon = raw_lon - _off_lon;

                // 5. 滤波输出
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