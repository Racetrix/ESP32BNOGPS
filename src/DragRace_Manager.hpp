#pragma once
#include <Arduino.h>

// 状态定义
enum DragState
{
    DRAG_IDLE,    // 等待状态 (未准备好)
    DRAG_READY,   // 准备就绪 (静止，等待发车)
    DRAG_RUNNING, // 正在加速
    DRAG_FINISHED // 完成 (显示成绩)
};

class DragRaceManager
{
private:
    DragState _state = DRAG_IDLE;

    uint32_t _startTime = 0;
    uint32_t _endTime = 0;
    float _resultTime = 0.0; // 秒

    // 配置参数
    const float SPEED_STOP_THRESHOLD = 1.5;    // 低于 1.5km/h 认为静止
    const float G_TRIGGER_THRESHOLD = 0.15;    // 纵向 G 值触发阈值 (0.15G 意味着明显的推背感)
    const float SPEED_TRIGGER_THRESHOLD = 5.0; // GPS 速度备用触发阈值
    const float TARGET_SPEED = 100.0;          // 目标速度

    // 防抖变量
    uint32_t _stopSince = 0; // 记录停下来的时刻
    bool _isStationary = false;

public:
    DragRaceManager() {}

    // 核心循环：传入当前 GPS 速度 (km/h) 和 纵向 G 值 (Longitudinal G)
    // 注意：longG 必须是去除了重力分量后的纯加速度，正数表示加速
    void update(float gpsSpeed, float longG)
    {
        switch (_state)
        {

        // --- 1. IDLE / READY 状态：检测是否静止 ---
        case DRAG_IDLE:
        case DRAG_READY:
            // 检测是否静止 (速度 < 1.5 且 G 值波动不大)
            if (gpsSpeed < SPEED_STOP_THRESHOLD)
            {
                if (_stopSince == 0)
                    _stopSince = millis();

                // 连续静止 2 秒以上，才进入 READY 状态 (防止急刹车未停稳就重置)
                if (millis() - _stopSince > 2000)
                {
                    if (_state != DRAG_READY)
                    {
                        _state = DRAG_READY;
                        Serial.println("[DRAG] READY TO LAUNCH!");
                    }
                }
            }
            else
            {
                // 车还在动
                _stopSince = 0;
                _state = DRAG_IDLE;
            }

            // 如果已经 READY，检测起步信号
            if (_state == DRAG_READY)
            {
                checkStartTrigger(gpsSpeed, longG);
            }
            break;

        // --- 2. RUNNING 状态：计时 & 测速 ---
        case DRAG_RUNNING:
            // A. 提前终止检查：如果起步后速度反而降到 0 (比如误触发)，重置
            if (gpsSpeed < 1.0 && (millis() - _startTime > 2000))
            {
                _state = DRAG_IDLE;
                Serial.println("[DRAG] False Start detected. Reset.");
                return;
            }

            // B. 完成检查：破百！
            if (gpsSpeed >= TARGET_SPEED)
            {
                _endTime = millis();
                _resultTime = (_endTime - _startTime) / 1000.0;
                _state = DRAG_FINISHED;

                Serial.print("[DRAG] FINISH! Time: ");
                Serial.println(_resultTime);
            }
            break;

        // --- 3. FINISHED 状态：等待减速复位 ---
        case DRAG_FINISHED:
            // 当速度降回到 10km/h 以下时，自动重置，准备下一次
            if (gpsSpeed < 10.0)
            {
                _state = DRAG_IDLE;
                _resultTime = 0;
                Serial.println("[DRAG] Resetting for next run...");
            }
            break;
        }
    }

    // 专门的起步检测逻辑
    void checkStartTrigger(float speed, float gForce)
    {
        bool triggered = false;

        // 判定 1: G 值突变 (最准)
        // 只有当速度很低时，才允许 G 值触发，防止行驶中误触
        if (gForce > G_TRIGGER_THRESHOLD)
        {
            triggered = true;
            Serial.printf("[DRAG] Triggered by G-Force: %.2f G\n", gForce);
        }

        // 判定 2: GPS 速度突变 (备用，防止 G 值传感器故障或起步太肉)
        // 如果 G 值没触发，但速度已经 5km/h 了，说明已经跑起来了
        else if (speed > SPEED_TRIGGER_THRESHOLD)
        {
            triggered = true;
            Serial.printf("[DRAG] Triggered by GPS: %.1f km/h\n", speed);
            // 补偿机制：如果是 GPS 触发的，说明已经晚了，扣除 200ms (经验值)
            // _startTime -= 200;
        }

        if (triggered)
        {
            _startTime = millis();
            _state = DRAG_RUNNING;
        }
    }

    // --- Getters 用于 UI 显示 ---

    DragState getState() { return _state; }

    // 获取实时计时 (秒)
    float getCurrentTime()
    {
        if (_state == DRAG_IDLE || _state == DRAG_READY)
            return 0.0;
        if (_state == DRAG_FINISHED)
            return _resultTime;
        return (millis() - _startTime) / 1000.0;
    }

    // 获取最终成绩
    float getResult() { return _resultTime; }

    // 是否准备好 (用于点亮 UI 上的 "READY" 灯)
    bool isReady() { return _state == DRAG_READY; }
};