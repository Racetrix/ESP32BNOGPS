#pragma once
#include <Arduino.h>
#include "FS.h"
#include "SD_MMC.h"
#include "GPS_Driver.hpp"
#include "IMU_Driver.hpp"

extern GPS_Driver gps;
extern IMU_Driver imu;

class DataLogger
{
private:
    File logFile;
    bool isRecording = false;

    // --- 缓冲策略 ---
    // 4KB 是 SD 卡的一个扇区大小，写入效率最高
    static const size_t BUF_SIZE = 4096;
    char buffer[BUF_SIZE];
    size_t bufOffset = 0;

    // 获取当前时间字符串 (用于文件名和CSV第一列)
    // 格式: 2025-12-26 13:38:05.964
    String getDateTimeString(bool forFilename)
    {
        if (!gps.tgps.date.isValid())
            return forFilename ? "/gps_no_time.csv" : "2000-01-01 00:00:00.000";

        char buf[32];
        if (forFilename)
        {
            // 文件名不能有冒号
            snprintf(buf, sizeof(buf), "/%04d%02d%02d_%02d%02d%02d.csv",
                     gps.tgps.date.year(), gps.tgps.date.month(), gps.tgps.date.day(),
                     gps.tgps.time.hour(), gps.tgps.time.minute(), gps.tgps.time.second());
        }
        else
        {
            // CSV 时间列
            snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                     gps.tgps.date.year(), gps.tgps.date.month(), gps.tgps.date.day(),
                     gps.tgps.time.hour(), gps.tgps.time.minute(), gps.tgps.time.second(),
                     gps.tgps.time.centisecond() * 10); // 简单模拟毫秒
        }
        return String(buf);
    }

    // 将缓冲区的数据真正写入 SD 卡
    void flushBuffer()
    {
        if (!logFile || bufOffset == 0)
            return;

        // 一次性写入大块数据
        logFile.write((uint8_t *)buffer, bufOffset);

        // 强制同步到物理介质 (防止断电丢数据)
        logFile.flush();

        // 重置缓冲区
        bufOffset = 0;
    }

public:
    // 开始录制
    bool start()
    {
        if (isRecording)
            return true;

        String fileName = getDateTimeString(true); // 生成文件名
        Serial.printf("Creating Log: %s\n", fileName.c_str());

        // 使用 "a" (追加) 模式，虽然是新文件
        logFile = SD_MMC.open(fileName, FILE_WRITE);
        if (!logFile)
        {
            Serial.println("❌ Failed to create file!");
            return false;
        }

        isRecording = true;
        bufOffset = 0;

        // 写入 CSV 表头
        // Time,Lat,Lon,Alt,Speed_kmh,Sats,Fix,Heading,Roll,Pitch,Lon_G,Lat_G
        logFile.println("Time,Lat,Lon,Alt,Speed_kmh,Sats,Fix,Heading,Roll,Pitch,Lon_G,Lat_G");
        return true;
    }

    // 记录一帧数据 (10Hz 调用)
    void log()
    {
        if (!isRecording)
            return;

        char line[128];

        // 关键修改：最后两个参数改为 imu.ax_raw 和 imu.ay_raw
        int len = snprintf(line, sizeof(line),
                           "%s,%.8f,%.8f,%.2f,%.2f,%d,%d,%.2f,%.2f,%.2f,%.3f,%.3f\n",
                           getDateTimeString(false).c_str(),
                           gps.tgps.location.lat(),
                           gps.tgps.location.lng(),
                           gps.tgps.altitude.meters(),
                           gps.getSpeed(),
                           gps.getSatellites(),
                           gps.tgps.location.isValid() ? 1 : 0,
                           gps.tgps.course.deg(),
                           0.0, 0.0,
                           imu.ax_raw, imu.ay_raw // <--- 这里改用原始数据！
        );

        if (bufOffset + len >= BUF_SIZE)
        {
            flushBuffer();
        }

        memcpy(buffer + bufOffset, line, len);
        bufOffset += len;
    }

    // 停止录制
    void stop()
    {
        if (!isRecording)
            return;

        flushBuffer(); // 把最后剩下的数据写进去
        logFile.close();
        isRecording = false;
        Serial.println("Log Saved & Closed.");
    }

    bool isActive() { return isRecording; }
};

DataLogger logger;