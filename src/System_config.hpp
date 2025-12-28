#pragma once
#include <Arduino.h>
#include <Preferences.h>

// 定义运行模式枚举
enum AppMode
{
    MODE_ROAM = 0, // 漫游模式
    MODE_TRACK = 1 // 赛道模式
};

class ConfigManager
{
private:
    Preferences prefs;
    const char *NS = "settings";

public:
    // --- 设置项 ---
    bool bluetooth_on = false;
    bool gps_10hz_mode = false;
    bool imu_swap_axis = false;
    bool imu_invert_x = false;
    bool imu_invert_y = false;

    // --- 运行时状态 ---
    AppMode current_mode = MODE_ROAM;
    bool is_running = false;

    // [新增] 记录开始那一刻的系统运行时间 (毫秒)
    uint32_t session_start_ms = 0;

    bool boot_into_usb = false;

    void load()
    {
        prefs.begin(NS, true);
        bluetooth_on = prefs.getBool("bt", false);
        gps_10hz_mode = prefs.getBool("gps10", false);
        imu_swap_axis = prefs.getBool("swap", false);
        imu_invert_x = prefs.getBool("invX", false);
        imu_invert_y = prefs.getBool("invY", false);
        boot_into_usb = prefs.getBool("usb_mode", false);
        prefs.end();
        Serial.println("Config Loaded");
    }

    void save()
    {
        prefs.begin(NS, false);
        prefs.putBool("bt", bluetooth_on);
        prefs.putBool("gps10", gps_10hz_mode);
        prefs.putBool("swap", imu_swap_axis);
        prefs.putBool("invX", imu_invert_x);
        prefs.putBool("invY", imu_invert_y);
        prefs.putBool("usb_mode", boot_into_usb);
        prefs.end();
        Serial.println("Config Saved");
    }
};

extern ConfigManager sys_cfg;