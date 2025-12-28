#pragma once
#include <Arduino.h>
#include "FS.h"
#include "SD_MMC.h"
#include "GPS_Driver.hpp"
#include "IMU_Driver.hpp"
#include <time.h>
#include <sys/time.h> // 用于设置系统时钟

extern GPS_Driver gps;
extern IMU_Driver imu;

class DataLogger
{
private:
    File logFile;
    bool isRecording = false;

    // --- 缓冲策略 ---
    static const size_t BUF_SIZE = 4096;
    char buffer[BUF_SIZE];
    size_t bufOffset = 0;

    // --- 核心：将 GPS 时间同步到 ESP32 系统内核 ---
    // 只有同步了系统时间，SD 卡文件的“修改日期”在电脑上才会显示正确
    void syncSystemTime()
    {
        if (!gps.tgps.date.isValid() || !gps.tgps.time.isValid())
            return;

        struct tm tm_gps = {0};
        tm_gps.tm_year = gps.tgps.date.year() - 1900;
        tm_gps.tm_mon = gps.tgps.date.month() - 1;
        tm_gps.tm_mday = gps.tgps.date.day();
        tm_gps.tm_hour = gps.tgps.time.hour();
        tm_gps.tm_min = gps.tgps.time.minute();
        tm_gps.tm_sec = gps.tgps.time.second();

        // 1. 设置系统时区 (CST-8 = UTC+8)
        // 注意：POSIX 格式里，"CST-8" 意味着本地时间比 UTC *早* 8小时，也就是东八区
        setenv("TZ", "CST-8", 1);
        tzset();

        // 2. 将 GPS 时间 (UTC) 设置为系统时间
        time_t t = mktime(&tm_gps); // mktime 会根据 TZ 环境变量反推 UTC 时间戳
        struct timeval now = {.tv_sec = t, .tv_usec = 0};
        settimeofday(&now, NULL);

        Serial.println("✅ System Clock Synced to GPS Time!");
    }

    // 获取当前时间字符串 (UTC+8)
    // 格式: 2025-12-28 22:39:05.123
    String getTimestampString()
    {
        // 如果 GPS 无效，返回默认
        if (!gps.tgps.date.isValid())
            return "2000-01-01 00:00:00.000";

        // 直接获取 ESP32 系统时间 (因为我们已经在 start 里同步过了)
        // 或者手动算 UTC+8 也可以，这里为了稳健还是手动算一遍
        struct tm tm_val = {0};
        tm_val.tm_year = gps.tgps.date.year() - 1900;
        tm_val.tm_mon = gps.tgps.date.month() - 1;
        tm_val.tm_mday = gps.tgps.date.day();
        tm_val.tm_hour = gps.tgps.time.hour();
        tm_val.tm_min = gps.tgps.time.minute();
        tm_val.tm_sec = gps.tgps.time.second();

        // 临时转 UTC+8
        time_t t_utc = mktime(&tm_val); // 这里假设 mktime 此时按 UTC 解析(取决于 setenv)
        // 修正：最稳妥的办法是手动加 8 小时秒数
        // 为了避免时区混乱，我们直接用原始数学计算文件名
        // 但为了代码复用，下面单独写文件名生成逻辑

        char buf[32];
        // 这里只是生成 CSV 里的字符串，我们简单加 8 小时处理显示
        int hour_local = (tm_val.tm_hour + 8) % 24;
        // 注意：这里简单的 hour+8 处理不了跨天，建议还是用 time.h 的 gmtime
        // 为确保万无一失，我们用标准库：

        time_t t_local = t_utc + 28800; // +8 hours
        struct tm *local_tm = gmtime(&t_local);

        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                 local_tm->tm_year + 1900, local_tm->tm_mon + 1, local_tm->tm_mday,
                 local_tm->tm_hour, local_tm->tm_min, local_tm->tm_sec,
                 gps.tgps.time.centisecond() * 10);
        return String(buf);
    }

    // 专门生成文件名: /session/session_20251228_2239.csv
    String generateFileName()
    {
        if (!gps.tgps.date.isValid())
            return "/session/session_no_gps.csv";

        struct tm tm_utc = {0};
        tm_utc.tm_year = gps.tgps.date.year() - 1900;
        tm_utc.tm_mon = gps.tgps.date.month() - 1;
        tm_utc.tm_mday = gps.tgps.date.day();
        tm_utc.tm_hour = gps.tgps.time.hour();
        tm_utc.tm_min = gps.tgps.time.minute();
        tm_utc.tm_sec = gps.tgps.time.second();

        // 设为 UTC0 计算时间戳
        setenv("TZ", "UTC0", 1);
        tzset();
        time_t t = mktime(&tm_utc);

        // +8 小时
        t += 28800;
        struct tm *local = gmtime(&t);

        char buf[64];
        snprintf(buf, sizeof(buf), "/session/session_%04d%02d%02d_%02d%02d.csv",
                 local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                 local->tm_hour, local->tm_min);

        return String(buf);
    }

    void flushBuffer()
    {
        if (!logFile || bufOffset == 0)
            return;
        logFile.write((uint8_t *)buffer, bufOffset);
        logFile.flush();
        bufOffset = 0;
    }

public:
    // 开始录制
    bool start()
    {
        if (isRecording)
            return true;

        // 1. 同步系统时间 (关键！让电脑能看到正确的文件创建日期)
        syncSystemTime();

        // 2. 确保文件夹存在
        if (!SD_MMC.exists("/session"))
        {
            Serial.println("Creating /session directory...");
            SD_MMC.mkdir("/session");
        }

        // 3. 生成文件名
        String fileName = generateFileName();
        Serial.printf("Creating Log: %s\n", fileName.c_str());

        // 4. 打开文件
        logFile = SD_MMC.open(fileName, FILE_WRITE);
        if (!logFile)
        {
            Serial.println("❌ Failed to create file!");
            return false;
        }

        isRecording = true;
        bufOffset = 0;

        // 5. 写入完整表头
        logFile.println("Time,Lat,Lon,Alt,Speed_kmh,Sats,Fix,Heading,Roll,Pitch,Acc_X,Acc_Y,Acc_Z,Gyro_X,Gyro_Y,Gyro_Z");
        return true;
    }

    // 记录一帧数据 (10Hz)
    void log()
    {
        if (!isRecording)
            return;

        char line[256]; // 稍微加大一点防止溢出

        // 格式化 CSV 行
        int len = snprintf(line, sizeof(line),
                           "%s,%.8f,%.8f,%.2f,%.2f,%d,%d,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,0.0,0.0,0.0\n",
                           getTimestampString().c_str(),
                           gps.tgps.location.lat(),
                           gps.tgps.location.lng(),
                           gps.tgps.altitude.meters(),
                           gps.getSpeed(),
                           gps.getSatellites(),
                           gps.tgps.location.isValid() ? 1 : 0,
                           gps.tgps.course.deg(),
                           0.0, 0.0,                          // Roll, Pitch (如果后续有算法算出姿态可填入)
                           imu.ax_raw, imu.ay_raw, imu.az_raw // 写入三轴加速度
                                                              // Gyro 数据暂无，填0
        );

        if (bufOffset + len >= BUF_SIZE)
        {
            flushBuffer();
        }

        memcpy(buffer + bufOffset, line, len);
        bufOffset += len;
    }

    void stop()
    {
        if (!isRecording)
            return;
        flushBuffer();
        logFile.close();
        isRecording = false;
        Serial.println("Log Saved & Closed.");
    }

    bool isActive() { return isRecording; }
};

DataLogger logger;