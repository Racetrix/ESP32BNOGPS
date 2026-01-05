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
    RACE_IDLE = 0,    // 闲置
    RACE_ARMED = 1,   // 预备 (已进入高精度检测区)
    RACE_RUNNING = 2, // 正在计时
    RACE_FINISHED = 3 // 完成
};

class TrackManager
{
private:
    TrackType type;
    GeoPoint startPoint;
    GeoPoint endPoint;

    bool _isArmed = false;

    // [修改] 触发半径收缩到 3.0米
    // 在 10Hz 下，200km/h 的车一帧跑 5.5米。
    // 3.0米半径意味着检测窗口直径 6.0米，勉强能兜住高速冲线。
    // 再小容易漏，再大容易误触。
    float triggerRadius = 3.0;
    const uint32_t LAP_COOLDOWN_MS = 5000;

    RaceState currentState = RACE_IDLE;
    uint32_t startTimeMs = 0;
    uint32_t lastTriggerTimeMs = 0;

    uint32_t currentLapTime = 0;
    uint32_t lastLapTime = 0;
    uint32_t bestLapTime = 0xFFFFFFFF;
    int lapCount = 0;

    double trackHeading = -1.0;

    // 极值检测变量
    float prevDistanceToStart = 99999.0;
    float prevDistanceToEnd = 99999.0;

    // [新增] 记录上一帧的时间戳，用于时间插值
    uint32_t prevTimeMs = 0;

    TrackEventCallback onRaceStartCB = NULL;
    TrackEventCallback onRaceFinishCB = NULL;
    TrackEventCallback onLapStartCB = NULL;
    TrackEventCallback onLapFinishCB = NULL;

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

    double getHeadingDiff(double h1, double h2)
    {
        double diff = abs(h1 - h2);
        if (diff > 180.0)
            diff = 360.0 - diff;
        return diff;
    }

public:
    TrackManager()
    {
        bestLapTime = 0xFFFFFFFF;
    }

    void attachOnStart(TrackEventCallback cb) { onRaceStartCB = cb; }
    void attachOnFinish(TrackEventCallback cb) { onRaceFinishCB = cb; }
    void attachOnLapStart(TrackEventCallback cb) { onLapStartCB = cb; }
    void attachOnLapFinish(TrackEventCallback cb) { onLapFinishCB = cb; }

    void setupTrack(TrackType t, float radius, double sLat, double sLon, double eLat, double eLon)
    {
        type = t;
        // 忽略传入的 radius，强制使用 3.0米 的高精度逻辑
        // 如果你需要兼容旧的大半径，可以把下面这行删掉
        triggerRadius = radius;

        startPoint = {sLat, sLon};
        if (type == TRACK_TYPE_CIRCUIT)
            endPoint = startPoint;
        else
            endPoint = {eLat, eLon};
        resetSession();
        _isArmed = false;
        Serial.printf("Track Setup: Mode=%d, Pro-Radius=%.1fm\n", type, triggerRadius);
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
        trackHeading = -1.0;
        prevDistanceToStart = 99999.0;
        prevDistanceToEnd = 99999.0;
        prevTimeMs = 0;
    }

    void enterStandbyMode()
    {
        resetSession();
        _isArmed = true;
        Serial.println("[TRACK] ARMED. Waiting (Pro-Mode)...");
    }

    void exitTrackMode()
    {
        _isArmed = false;
        currentState = RACE_IDLE;
        Serial.println("[TRACK] DISARMED.");
    }

    // [核心函数]
    void update(double currLat, double currLon, double currHeading, float currSpeedKmh, uint32_t now)
    {
        if (!_isArmed || (abs(currLat) < 0.1 && abs(currLon) < 0.1))
            return;

        float distToStart = getDistance(currLat, currLon, startPoint.lat, startPoint.lon);
        float distToEnd = getDistance(currLat, currLon, endPoint.lat, endPoint.lon);

        // --- 1. 检测比赛开始 ---
        if (currentState == RACE_IDLE || currentState == RACE_ARMED)
        {
            if (distToStart < triggerRadius) // < 3.0米 才会进入判定
            {
                // 提高速度门限到 8km/h，防止静止漂移误触
                if (currSpeedKmh > 8.0)
                {
                    // 距离开始变大 (说明上一帧就是最近点)
                    if (currentState == RACE_ARMED && distToStart > prevDistanceToStart)
                    {
                        // [严格检查] 只有当最近点真的小于 1.5米 时才触发
                        // 这杜绝了只是擦过 3米 圈边缘导致的误触
                        if (prevDistanceToStart < 1.5 && (now - lastTriggerTimeMs > LAP_COOLDOWN_MS))
                        {
                            currentState = RACE_RUNNING;

                            // [回溯时间补偿]
                            // 真实的过线时间其实发生在“上一帧”和“这一帧”之间
                            // 简单起见，我们认为上一帧时刻 (prevTimeMs) 就是最近点时刻
                            // 这样精度比直接用 now 要准 100ms
                            uint32_t exactStartTime = (prevTimeMs > 0) ? prevTimeMs : now;

                            startTimeMs = exactStartTime;
                            lastTriggerTimeMs = now; // 冷却计时还是用 now
                            currentLapTime = 0;
                            lapCount = 1;
                            trackHeading = currHeading;

                            Serial.printf("🏁 START! (MinDist: %.2fm, TimeFix: -%dms)\n", prevDistanceToStart, now - exactStartTime);
                            audioDriver.play("/mp3/race_start.mp3");

                            if (onRaceStartCB != NULL)
                                onRaceStartCB();
                            if (onLapStartCB != NULL)
                                onLapStartCB();
                        }
                    }
                    else
                    {
                        currentState = RACE_ARMED;
                    }
                }
            }
            else
            {
                currentState = RACE_IDLE;
            }
        }

        // --- 2. 检测过线/终点 ---
        else if (currentState == RACE_RUNNING)
        {
            currentLapTime = now - startTimeMs;

            if (distToEnd < triggerRadius && (now - lastTriggerTimeMs > LAP_COOLDOWN_MS))
            {
                if (currSpeedKmh > 8.0)
                {
                    bool headingOK = true;
                    if (trackHeading >= 0)
                    {
                        if (getHeadingDiff(currHeading, trackHeading) > 90.0)
                            headingOK = false;
                    }

                    // 同样加上严格距离检查 (< 1.5m)
                    if (headingOK && distToEnd > prevDistanceToEnd && prevDistanceToEnd < 1.5)
                    {
                        // [回溯时间补偿]
                        uint32_t exactFinishTime = (prevTimeMs > 0) ? prevTimeMs : now;

                        // 计算修正后的圈速
                        // 圈速 = (结束时刻 - 开始时刻)
                        // 注意：startTimeMs 已经是修正过的了，所以这里直接减
                        uint32_t correctedLapTime = exactFinishTime - startTimeMs;

                        // 更新基准时间
                        lastTriggerTimeMs = now;
                        lastLapTime = correctedLapTime;

                        if (lastLapTime < bestLapTime)
                            bestLapTime = lastLapTime;
                        Serial.printf("🏁 LAP! Time: %.3fs (MinDist: %.2fm)\n", lastLapTime / 1000.0, prevDistanceToEnd);

                        if (type == TRACK_TYPE_SPRINT)
                        {
                            currentState = RACE_FINISHED;
                            if (onLapFinishCB != NULL)
                                onLapFinishCB();
                            if (onRaceFinishCB != NULL)
                                onRaceFinishCB();
                        }
                        else
                        {
                            if (onLapFinishCB != NULL)
                                onLapFinishCB();

                            // 这一圈的结束时间，就是下一圈的开始时间！
                            startTimeMs = exactFinishTime;
                            currentLapTime = 0;
                            lapCount++;

                            if (onLapStartCB != NULL)
                                onLapStartCB();
                        }
                    }
                }
            }
        }

        // 更新历史记录
        prevDistanceToStart = distToStart;
        prevDistanceToEnd = distToEnd;
        prevTimeMs = now; // [关键] 记录这一帧的时间
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

    bool isTrackSetup() { return (abs(startPoint.lat) > 0.1); }
    int getCurrentTrackType() { return (int)type; }
    uint32_t getCurrentLapElapsed() { return (currentState == RACE_RUNNING) ? (millis() - startTimeMs) : 0; }
    void getStartPoint(double &lat, double &lon)
    {
        lat = startPoint.lat;
        lon = startPoint.lon;
    }
    bool isArmed() { return _isArmed; }
    float getTriggerRadius() { return triggerRadius; }
    String getCurrentTimeStr() { return getFormattedTime(currentLapTime); }
    String getLastLapStr() { return getFormattedTime(lastLapTime); }
    String getBestLapStr() { return getFormattedTime(bestLapTime); }
    int getLapCount() { return lapCount; }
    bool isRunning() { return currentState == RACE_RUNNING; }
};

extern TrackManager trackMgr;