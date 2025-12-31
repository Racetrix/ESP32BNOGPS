#pragma once
#include <Arduino.h>
#include "FS.h"
#include "SD_MMC.h"
#include "GPS_Driver.hpp"
#include "IMU_Driver.hpp"
#include <time.h>
#include <sys/time.h>

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

    // --- 时间同步逻辑 (保持原样) ---
    void syncSystemTime()
    {
        if (!gps.tgps.date.isValid() || !gps.tgps.time.isValid())
            return;

        struct tm tm_utc = {0};
        tm_utc.tm_year = gps.tgps.date.year() - 1900;
        tm_utc.tm_mon = gps.tgps.date.month() - 1;
        tm_utc.tm_mday = gps.tgps.date.day();
        tm_utc.tm_hour = gps.tgps.time.hour();
        tm_utc.tm_min = gps.tgps.time.minute();
        tm_utc.tm_sec = gps.tgps.time.second();

        setenv("TZ", "UTC0", 1);
        tzset();
        time_t t_utc = mktime(&tm_utc);
        time_t t_beijing = t_utc + 28800; // +8 小时

        struct timeval now = {.tv_sec = t_beijing, .tv_usec = 0};
        settimeofday(&now, NULL);

        setenv("TZ", "UTC0", 1);
        tzset();
        Serial.println("✅ System Clock Force Set to Beijing Time");
    }

    String getTimestampString()
    {
        if (!gps.tgps.date.isValid())
            return "2000-01-01 00:00:00.000";

        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec;
        struct tm *tm_local = gmtime(&now);

        char buf[32];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                 tm_local->tm_year + 1900, tm_local->tm_mon + 1, tm_local->tm_mday,
                 tm_local->tm_hour, tm_local->tm_min, tm_local->tm_sec,
                 (int)(tv.tv_usec / 1000));
        return String(buf);
    }

    String generateFileName()
    {
        if (!gps.tgps.date.isValid())
            return "/session/session_no_gps.csv";

        struct timeval tv;
        gettimeofday(&tv, NULL);
        time_t now = tv.tv_sec;
        struct tm *local = gmtime(&now);

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
    bool start()
    {
        if (isRecording)
            return true;

        syncSystemTime();

        if (!SD_MMC.exists("/session"))
        {
            SD_MMC.mkdir("/session");
        }

        String fileName = generateFileName();
        Serial.printf("Creating Log: %s\n", fileName.c_str());

        logFile = SD_MMC.open(fileName, FILE_WRITE);
        if (!logFile)
        {
            Serial.println("❌ Failed to create file!");
            return false;
        }

        isRecording = true;
        bufOffset = 0;

        // [修改] 表头：
        // 1. 保留了前面的 GPS 数据 (Time, Lat, Lon, Alt, Speed, Sats, Fix)
        // 2. 删除了原来的 Acc_X, Acc_Y, Acc_Z, Gyro...
        // 3. 替换为你要求的 5 个值: Heading, Roll, Pitch, Lon_G, Lat_G
        logFile.println("Time,Lat,Lon,Alt,Speed_kmh,Sats,Fix,Heading,Roll,Pitch,Lon_G,Lat_G");

        return true;
    }

    void log()
    {
        if (!isRecording)
            return;

        char line[256];

        // [修改] 数据填充：
        // 注意：Heading 现在优先使用 IMU 的数据(刷新率高)，如果 IMU 没初始化可以用 GPS 的顶替
        // 这里默认全部使用 IMU 算出来的数据
        int len = snprintf(line, sizeof(line),
                           "%s,%.8f,%.8f,%.2f,%.2f,%d,%d,%.1f,%.1f,%.1f,%.2f,%.2f\n",
                           getTimestampString().c_str(),        // 1. Time
                           gps.tgps.location.lat(),             // 2. Lat
                           gps.tgps.location.lng(),             // 3. Lon
                           gps.tgps.altitude.meters(),          // 4. Alt
                           gps.getSpeed(),                      // 5. Speed
                           gps.getSatellites(),                 // 6. Sats
                           gps.tgps.location.isValid() ? 1 : 0, // 7. Fix

                           // --- 这里开始是你要求的 5 个新参数 ---
                           imu.heading, // 8. Heading (来自 IMU)
                           imu.roll,    // 9. Roll
                           imu.pitch,   // 10. Pitch
                           imu.lon_g,   // 11. Lon_G (纵向 G)
                           imu.lat_g    // 12. Lat_G (横向 G)
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