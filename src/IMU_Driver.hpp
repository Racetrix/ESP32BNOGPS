#pragma once
#include <Arduino.h>
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

    // [修改] 增加长度以读取 Z 轴数据 (原20 -> 22)
    // 假设 Z轴数据紧跟在 Y轴后面 (寄存器地址连续)
    const uint8_t DATA_LEN = 22;

    const uint8_t REG_OPR_MODE = 0x3D;
    const uint8_t OPR_MODE_NDOF = 0x0C;

    const float ALPHA = 0.15;

    float _off_head = 0, _off_roll = 0, _off_pit = 0;
    float _off_lon = 0, _off_lat = 0;

    // --- [核心优化] 定义函数指针类型 ---
    // [修改] 增加 float az 参数，现在接收 6 个参数
    typedef void (IMU_Driver::*ProcessDataFunc)(float, float, float, float, float, float);

    // 当前正在使用的处理函数
    ProcessDataFunc currentProcessor = NULL;

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

        // 初始化配置
        applyConfig();

        isConnected = true;
    }

    // --- 当设置改变时调用 ---
    void applyConfig()
    {
        // 根据 sys_cfg.mount_orientation 切换处理逻辑
        // 0: Flat (平躺), 1: Vertical (竖立)
        if (sys_cfg.mount_orientation == 1)
        {
            currentProcessor = &IMU_Driver::process_Vertical;
        }
        else
        {
            currentProcessor = &IMU_Driver::process_Flat;
        }
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

    // --- 两个专用的处理函数 (编译器内联优化) ---

    // 逻辑 A: 平躺模式 (Flat)
    // X: 左右 (Lat), Y: 前后 (Lon), Z: 重力
    void process_Flat(float h, float r, float p, float ax, float ay, float az)
    {
        raw_lat = ax;
        raw_lon = ay; // 使用 Y 轴作为纵向 G

        raw_roll = r;
        raw_pit = p;

        finishProcessing(h);
    }

    // 逻辑 B: 竖立模式 (Vertical)
    // X: 左右 (Lat), Y: 重力, Z: 前后 (Lon)
    void process_Vertical(float h, float r, float p, float ax, float ay, float az)
    {
        raw_lat = ay;

        // [关键] 在竖立模式下，Z 轴代替了原来的 Y 轴成为前后方向
        // 注意：根据安装方向（屏幕朝后还是朝前），这里可能需要 az 或 -az
        // 默认假设屏幕面向驾驶员，加速时 Z 轴感应到正向 G (视具体传感器坐标系而定，反了加负号即可)
        raw_lon = az;

        // 竖立时，Roll 和 Pitch 的物理意义也会交换，这里暂且保持原样或根据需求调整
        // 通常竖立时，原本的 Yaw 变成了 Roll，原本的 Pitch 还是 Pitch (视旋转轴而定)
        // 这里仅处理 G 值，角度暂传原值
        raw_roll = r;
        raw_pit = p;

        finishProcessing(h);
    }

    // 公共的后续步骤
    inline void finishProcessing(float h)
    {
        if (sys_cfg.imu_swap_axis)
        {
            float temp = raw_lat;
            raw_lat = raw_lon;
            raw_lon = temp;

            // 注意：通常交换轴向也会导致 Roll 和 Pitch 交换
            // 如果你需要连角度一起交换，把下面两行注释解开
            // float temp_ang = raw_roll;
            // raw_roll = raw_pit;
            // raw_pit = temp_ang;
        }

        // 反转逻辑 (基于系统配置)
        if (sys_cfg.imu_invert_x)
        {
            raw_lat = -raw_lat;
            raw_roll = -raw_roll;
        }

        // 这里控制的是纵向反转 (无论是 Y 还是 Z)
        if (sys_cfg.imu_invert_y)
        {
            raw_lon = -raw_lon;
            raw_pit = -raw_pit;
        }

        // 赋值 Head
        raw_head = h;

        // 校准
        roll = raw_roll - _off_roll;
        pitch = raw_pit - _off_pit;
        heading = raw_head - _off_head;
        while (heading < 0)
            heading += 360.0;
        while (heading >= 360)
            heading -= 360.0;

        float c_lat = raw_lat - _off_lat;
        float c_lon = raw_lon - _off_lon;

        // 滤波
        lat_g = (lat_g * (1.0 - ALPHA)) + (c_lat * ALPHA);
        lon_g = (lon_g * (1.0 - ALPHA)) + (c_lon * ALPHA);
    }

    void update()
    {
        // [修改] 检查缓冲区长度是否满足新的 DATA_LEN (22)
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

                // 解析角度
                int16_t h_int = (int16_t)((buf[1] << 8) | buf[0]);
                int16_t r_int = (int16_t)((buf[3] << 8) | buf[2]);
                int16_t p_int = (int16_t)((buf[5] << 8) | buf[4]);
                float t_h = h_int / 16.0;
                float t_r = r_int / 16.0;
                float t_p = p_int / 16.0;

                // 解析加速度 X, Y
                int16_t x_int = (int16_t)((buf[15] << 8) | buf[14]);
                int16_t y_int = (int16_t)((buf[17] << 8) | buf[16]);

                // [新增] 解析加速度 Z (假设在 bytes 18-19)
                int16_t z_int = (int16_t)((buf[19] << 8) | buf[18]);

                float t_ax = (x_int / 100.0) / 9.81;
                float t_ay = (y_int / 100.0) / 9.81;
                float t_az = (z_int / 100.0) / 9.81; // 转换 Z 轴

                // --- [核心优化] 直接通过指针调用，传入 6 个参数 ---
                if (currentProcessor)
                {
                    (this->*currentProcessor)(t_h, t_r, t_p, t_ax, t_ay, t_az);
                }

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
            // 请求数据
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