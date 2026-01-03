#pragma once
#include <Arduino.h>
#include <vector>
#include "Audio_Driver.hpp"
// 定义回调函数类型
typedef void (*TrackEventCallback)();

enum TrackType
{
    TRACK_TYPE_CIRCUIT = 0,
    TRACK_TYPE_SPRINT = 1
};

struct GeoPoint
{
    double lat;
    double lon;
};

enum RaceState
{
    RACE_IDLE = 0,
    RACE_RUNNING = 1,
    RACE_FINISHED = 2
};

class TrackManager
{
private:
    // Audio_Driver  audio;
    TrackType type;
    GeoPoint startPoint;
    GeoPoint endPoint;
    bool _isArmed = false;

    // --- [修改] 这里的半径改为变量，不再是 const ---
    float triggerRadius = 10.0;            // 默认 10米，防止用户没设
    const uint32_t LAP_COOLDOWN_MS = 5000; // 5秒冷却，避免反复触发

    // 运行时变量
    RaceState currentState = RACE_IDLE;
    uint32_t startTimeMs = 0;
    uint32_t lastTriggerTimeMs = 0;

    uint32_t currentLapTime = 0;
    uint32_t lastLapTime = 0;
    uint32_t bestLapTime = 0xFFFFFFFF;
    int lapCount = 0;

    TrackEventCallback onRaceStartCB = NULL;
    TrackEventCallback onRaceFinishCB = NULL;

    // Haversine 距离计算 (单位: 米)
    float getDistance(double lat1, double lon1, double lat2, double lon2)
    {
        const double R = 6371000.0;
        double dLat = (lat2 - lat1) * DEG_TO_RAD;
        double dLon = (lon2 - lon1) * DEG_TO_RAD;
        double a = sin(dLat / 2) * sin(dLat / 2) +
                   cos(lat1 * DEG_TO_RAD) * cos(lat2 * DEG_TO_RAD) *
                       sin(dLon / 2) * sin(dLon / 2);
        double c = 2 * atan2(sqrt(a), sqrt(1 - a));
        return (float)(R * c);
    }

public:
    TrackManager()
    {
        bestLapTime = 0xFFFFFFFF;
    }

    // --- [新增] 注册回调函数的接口 ---
    void attachOnStart(TrackEventCallback cb) { onRaceStartCB = cb; }
    void attachOnFinish(TrackEventCallback cb) { onRaceFinishCB = cb; }

    // --- [修改] 初始化函数，增加 radius 参数 ---
    void setupTrack(TrackType t, float radius, double sLat, double sLon, double eLat, double eLon)
    {
        type = t;
        triggerRadius = radius; // 设置自定义半径
        startPoint = {sLat, sLon};

        if (type == TRACK_TYPE_CIRCUIT)
        {
            endPoint = startPoint;
        }
        else
        {
            endPoint = {eLat, eLon};
        }

        resetSession();

        _isArmed = false;
        Serial.printf("Track Setup: Mode=%d, Radius=%.1fm\n", type, triggerRadius);
        Serial.printf("Start: %.6f, %.6f\n", sLat, sLon);
    }

    void resetSession()
    {
        currentState = RACE_IDLE;
        startTimeMs = 0;
        lastTriggerTimeMs = 0;
        currentLapTime = 0;
        lastLapTime = 0;
        bestLapTime = 0xFFFFFFFF;
        lapCount = 0;
    }

    void enterStandbyMode()
    {
        resetSession();
        _isArmed = true; // 开启保险，允许检测起点
        Serial.println("[TRACK] System ARMED. Waiting for start trigger...");
    }

    // [新增] 退出模式 (当用户点击 Cancel 退出等待界面时调用)
    void exitTrackMode()
    {
        _isArmed = false; // 关上保险
        currentState = RACE_IDLE;
        Serial.println("[TRACK] System DISARMED.");
    }

    void update(double currLat, double currLon, uint32_t now)
    {
        if (!_isArmed || (abs(currLat) < 0.1 && abs(currLon) < 0.1))
            return;

        float distToStart = getDistance(currLat, currLon, startPoint.lat, startPoint.lon);
        float distToEnd = getDistance(currLat, currLon, endPoint.lat, endPoint.lon);

        // A. 还没开始 -> 检测起点
        if (currentState == RACE_IDLE)
        {
            if (distToStart < triggerRadius)
            {
                if (now - lastTriggerTimeMs > LAP_COOLDOWN_MS)
                {
                    currentState = RACE_RUNNING;
                    startTimeMs = now;
                    lastTriggerTimeMs = now;
                    currentLapTime = 0; // [新增] 强制归零
                    lapCount = 1;

                    Serial.println("🏁 RACE START!");
                    audioDriver.play("/mp3/race_start.mp3");
                    if (onRaceStartCB != NULL)
                        onRaceStartCB();
                }
            }
        }

        // B. 正在计时 -> 检测终点/新的一圈
        else if (currentState == RACE_RUNNING)
        {
            // 实时更新当前圈时间
            currentLapTime = now - startTimeMs;

            // 只有过了冷却时间才允许触发（防止在起跑线来回抖动触发）
            if (distToEnd < triggerRadius && (now - lastTriggerTimeMs > LAP_COOLDOWN_MS))
            {
                lastTriggerTimeMs = now;
                lastLapTime = currentLapTime; // 保存上一圈成绩

                if (lastLapTime < bestLapTime)
                    bestLapTime = lastLapTime;

                Serial.printf("🏁 Lap/Finish! Time: %.3fs\n", lastLapTime / 1000.0);

                // 根据模式决定是“结束”还是“下一圈”
                if (type == TRACK_TYPE_SPRINT)
                {
                    currentState = RACE_FINISHED;
                    if (onRaceFinishCB != NULL)
                        onRaceFinishCB();
                }
                else
                {
                    // === [核心修复] 跑圈模式逻辑 ===
                    startTimeMs = now;  // 重置起跑时间为“现在”
                    currentLapTime = 0; // 当前圈耗时立刻归零
                    lapCount++;         // 圈数+1

                    // 这里不需要调用 Finish 回调，因为还在跑
                    // 但你可以加一个 playBeep() 提示过线
                }
            }
        }
    }

    String getFormattedTime(uint32_t ms)
    {
        if (ms == 0xFFFFFFFF || ms == 0)
            return "--:--.---";
        int minutes = ms / 60000;
        int seconds = (ms % 60000) / 1000;
        int millisec = ms % 1000;
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d.%03d", minutes, seconds, millisec);
        return String(buf);
    }

    // [新增] 判断当前是否配置了赛道
    bool isTrackSetup()
    {
        // 只要起点坐标有效 (> 0.1)，就视为已配置赛道
        return (abs(startPoint.lat) > 0.1 && abs(startPoint.lon) > 0.1);
    }

    // [新增] 获取当前赛道类型 (配合上面的判断使用)
    int getCurrentTrackType()
    {
        return (int)type;
    }
    uint32_t getCurrentLapElapsed()
    {
        if (currentState == RACE_RUNNING)
        {
            // 实时计算，保证毫秒级平滑
            return millis() - startTimeMs;
        }
        return 0;
    }
    void getStartPoint(double &lat, double &lon)
    {
        lat = startPoint.lat;
        lon = startPoint.lon;
    }

    bool isArmed()
    {
        return _isArmed;
    }

    // 获取当前赛道配置的半径
    float getTriggerRadius() { return triggerRadius; }
    String getCurrentTimeStr() { return getFormattedTime(currentLapTime); }
    String getLastLapStr() { return getFormattedTime(lastLapTime); }
    String getBestLapStr() { return getFormattedTime(bestLapTime); }
    int getLapCount() { return lapCount; }
    bool isRunning() { return currentState == RACE_RUNNING; }
};

extern TrackManager trackMgr;