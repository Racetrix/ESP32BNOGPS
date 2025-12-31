#pragma once
#include <Arduino.h>
#include <Preferences.h>

enum AppMode
{
    MODE_ROAM = 0,
    MODE_TRACK = 1
};

class ConfigManager
{
private:
    Preferences prefs;
    const char *NS = "settings";

public:
    // --- 基础设置 ---
    bool bluetooth_on = false;
    bool gps_10hz_mode = false;
    uint8_t volume = 10;
    bool boot_into_usb = false;

    // --- [修复] IMU 轴向配置  ---
    bool imu_swap_axis = false; // 交换 XY 轴
    bool imu_invert_x = false;  // 反转 X 轴方向
    bool imu_invert_y = false;  // 反转 Y 轴方向

    // --- 校准偏移量 ---
    float offset_lon = 0.0f; // 纵向 G
    float offset_lat = 0.0f; // 横向 G
    float offset_heading = 0.0f;
    float offset_roll = 0.0f;
    float offset_pitch = 0.0f;

    // --- 运行时状态 ---
    AppMode current_mode = MODE_ROAM;
    bool is_running = false;
    uint32_t session_start_ms = 0;

    void load()
    {
        prefs.begin(NS, true);
        bluetooth_on = prefs.getBool("bt", false);
        gps_10hz_mode = prefs.getBool("gps10", false);
        volume = prefs.getUChar("vol", 10);
        boot_into_usb = prefs.getBool("usb_mode", false);

        // [修复] 读取轴向配置
        imu_swap_axis = prefs.getBool("swap", false);
        imu_invert_x = prefs.getBool("invX", false);
        imu_invert_y = prefs.getBool("invY", false);

        // 读取偏移量
        offset_lon = prefs.getFloat("off_lon", 0.0f);
        offset_lat = prefs.getFloat("off_lat", 0.0f);
        offset_heading = prefs.getFloat("off_head", 0.0f);
        offset_roll = prefs.getFloat("off_roll", 0.0f);
        offset_pitch = prefs.getFloat("off_pit", 0.0f);

        prefs.end();
        Serial.println("Config Loaded");
    }

    void save()
    {
        prefs.begin(NS, false);
        prefs.putBool("bt", bluetooth_on);
        prefs.putBool("gps10", gps_10hz_mode);
        prefs.putBool("usb_mode", boot_into_usb);
        prefs.putUChar("vol", volume);

        // [修复] 保存轴向配置
        prefs.putBool("swap", imu_swap_axis);
        prefs.putBool("invX", imu_invert_x);
        prefs.putBool("invY", imu_invert_y);

        // 保存偏移量
        prefs.putFloat("off_lon", offset_lon);
        prefs.putFloat("off_lat", offset_lat);
        prefs.putFloat("off_head", offset_heading);
        prefs.putFloat("off_roll", offset_roll);
        prefs.putFloat("off_pit", offset_pitch);

        prefs.end();
        Serial.println("Config Saved");
    }
};

extern ConfigManager sys_cfg;