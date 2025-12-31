#pragma once

#include <lvgl.h>
#include "System_Config.hpp"
#include "Audio_Driver.hpp"
#include "IMU_Driver.hpp"

extern void screen_gesture_event_cb(lv_event_t *e);
extern lv_obj_t *ui_ScreenMain;

lv_obj_t *ui_ScreenSettings;

// --- 事件回调 ---
// [新增] 校准按钮的回调函数
void btn_calibrate_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_label_set_text(label, "Sampling...");
    lv_timer_handler(); // 刷新 UI

    // 准备累加器
    float sum_h = 0, sum_r = 0, sum_p = 0;
    float sum_lon = 0, sum_lat = 0;
    int samples = 50;

    Serial.println("Start Calibration (5-Axis)...");

    for (int i = 0; i < samples; i++)
    {
        float h, r, p, lon, lat;
        // 获取当前的“原始绝对值”
        imu.getRawValues(h, r, p, lon, lat);

        sum_h += h;
        sum_r += r;
        sum_p += p;
        sum_lon += lon;
        sum_lat += lat;

        // 注意：Heading如果正好在 0/360 边界跳变（比如 359, 1, 358...），
        // 简单的算术平均会出错。
        // 但我们在做“调平”时通常假设车是静止不动的，所以数据很稳，直接平均没问题。

        delay(10);
    }

    // 计算平均值 -> 这就是我们的“零点偏移量”
    sys_cfg.offset_heading = sum_h / samples;
    sys_cfg.offset_roll = sum_r / samples;
    sys_cfg.offset_pitch = sum_p / samples;
    sys_cfg.offset_lon = sum_lon / samples;
    sys_cfg.offset_lat = sum_lat / samples;

    // 保存到 NVS
    sys_cfg.save();

    // 立即生效
    imu.setAllOffsets(
        sys_cfg.offset_heading,
        sys_cfg.offset_roll,
        sys_cfg.offset_pitch,
        sys_cfg.offset_lon,
        sys_cfg.offset_lat);

    Serial.println("Calibration Done!");
    lv_label_set_text(label, "Done!");
    
}
// 音量滑块回调
void slider_volume_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        // 拖动时实时生效，听声音
        audioDriver.setVolume(val);
        sys_cfg.volume = val;
    }
    else if (code == LV_EVENT_RELEASED)
    {
        // 松手时保存
        sys_cfg.save();
        Serial.printf("Volume saved: %d\n", val);
    }
}

// 蓝牙开关回调
void sw_bt_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    sys_cfg.bluetooth_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sys_cfg.save();
    Serial.println(sys_cfg.bluetooth_on ? "BT: ON" : "BT: OFF");
}

// GPS 回调
void sw_gps_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    sys_cfg.gps_10hz_mode = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sys_cfg.save();
}

// IMU 回调
void sw_imu_swap_event_cb(lv_event_t *e)
{
    sys_cfg.imu_swap_axis = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    sys_cfg.save();
}
void sw_imu_inv_x_event_cb(lv_event_t *e)
{
    sys_cfg.imu_invert_x = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    sys_cfg.save();
}
void sw_imu_inv_y_event_cb(lv_event_t *e)
{
    sys_cfg.imu_invert_y = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    sys_cfg.save();
}

void btn_settings_back_event_cb(lv_event_t *e)
{
    lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

void btn_usb_mode_event_cb(lv_event_t *e)
{
    sys_cfg.boot_into_usb = true;
    sys_cfg.save();
    Serial.println("Rebooting into USB Mode...");
    ESP.restart();
}

// 辅助函数：创建普通设置行
void create_setting_item(lv_obj_t *parent, const char *text, bool current_state, lv_event_cb_t cb)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 300, 50);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE); // 这一行本身不滚动，但也防止干扰

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 5, 0);

    lv_obj_t *sw = lv_switch_create(cont);
    if (current_state)
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -5, 0);
}

// 辅助函数：创建音量滑块行
void create_volume_item(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 300, 80);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, "Volume");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 5, 0);

    // 滑块
    lv_obj_t *slider = lv_slider_create(cont);
    lv_obj_set_size(slider, 280, 15);                  // 加宽一点，方便手指按
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, -10); // 居中放置
    lv_slider_set_range(slider, 0, 21);
    lv_slider_set_value(slider, sys_cfg.volume, LV_ANIM_OFF);

    // 增加滑块旋钮(Knob)的大小，让手指更容易抓住
    lv_obj_set_style_pad_all(slider, 2, LV_PART_KNOB); // 让旋钮看起来大一点

    lv_obj_set_style_bg_color(slider, lv_color_hex(0x00AEEF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x555555), LV_PART_MAIN);

    lv_obj_add_event_cb(slider, slider_volume_event_cb, LV_EVENT_ALL, NULL);
}

// --- 构建设置页面 ---
void build_settings_page()
{
    if (ui_ScreenSettings)
        return;

    ui_ScreenSettings = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenSettings, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ScreenSettings, LV_OPA_COVER, LV_PART_MAIN);
    // 禁止整个屏幕滚动，只让里面的 list 滚
    lv_obj_set_scrollbar_mode(ui_ScreenSettings, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ui_ScreenSettings, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(ui_ScreenSettings, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    // 顶部标题
    lv_obj_t *header = lv_obj_create(ui_ScreenSettings);
    lv_obj_set_size(header, 320, 40);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00AEEF), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 5, 0);

    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 70, 30);
    lv_obj_align(btn_back, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(btn_back, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_back, btn_settings_back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "BACK");
    lv_obj_center(lbl_back);

    // 设置列表容器
    lv_obj_t *list_cont = lv_obj_create(ui_ScreenSettings);
    lv_obj_set_size(list_cont, 320, 200);
    lv_obj_align(list_cont, LV_ALIGN_TOP_MID, 0, 45);

    // 样式调整
    lv_obj_set_style_bg_color(list_cont, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list_cont, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(list_cont, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(list_cont, 0, LV_PART_MAIN);

    // 消除默认 Padding，确保宽度 300 的子对象不会撑出滚动条
    lv_obj_set_style_pad_all(list_cont, 10, LV_PART_MAIN);

    lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list_cont, 10, LV_PART_MAIN);

    // 【核心修复】强制只允许垂直滚动！
    // 这样左右滑动的操作就会全部交给内部的滑块，而不会触发页面滚动
    lv_obj_set_scroll_dir(list_cont, LV_DIR_VER);

    // --- 添加设置项 ---
    create_volume_item(list_cont);

    create_setting_item(list_cont, "Bluetooth", sys_cfg.bluetooth_on, sw_bt_event_cb);
    create_setting_item(list_cont, "GPS 10Hz Mode (Log)", sys_cfg.gps_10hz_mode, sw_gps_event_cb);
    create_setting_item(list_cont, "Swap G-Ball Axis (X/Y)", sys_cfg.imu_swap_axis, sw_imu_swap_event_cb);
    create_setting_item(list_cont, "Invert G-Ball X", sys_cfg.imu_invert_x, sw_imu_inv_x_event_cb);
    create_setting_item(list_cont, "Invert G-Ball Y", sys_cfg.imu_invert_y, sw_imu_inv_y_event_cb);

    // [新增] 添加校准按钮
    lv_obj_t *cont_cali = lv_obj_create(list_cont);
    lv_obj_set_size(cont_cali, 300, 50);
    lv_obj_set_style_bg_color(cont_cali, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont_cali, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont_cali, 0, LV_PART_MAIN);
    lv_obj_clear_flag(cont_cali, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_cali = lv_label_create(cont_cali);
    lv_label_set_text(lbl_cali, "Zero Calibration"); // 归零/调平
    lv_obj_set_style_text_color(lbl_cali, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(lbl_cali, LV_ALIGN_LEFT_MID, 5, 0);

    lv_obj_t *btn_cali = lv_btn_create(cont_cali);
    lv_obj_set_size(btn_cali, 100, 30);
    lv_obj_align(btn_cali, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(btn_cali, lv_color_hex(0xFF5722), LV_PART_MAIN); // 橙色按钮醒目一点
    lv_obj_add_event_cb(btn_cali, btn_calibrate_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_btn_cali = lv_label_create(btn_cali);
    lv_label_set_text(lbl_btn_cali, "LEVEL");
    lv_obj_center(lbl_btn_cali);

    // USB 模式按钮
    lv_obj_t *cont_usb = lv_obj_create(list_cont);
    lv_obj_set_size(cont_usb, 300, 50);
    lv_obj_set_style_bg_color(cont_usb, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont_usb, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(cont_usb, 0, LV_PART_MAIN);
    lv_obj_clear_flag(cont_usb, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_usb = lv_label_create(cont_usb);
    lv_label_set_text(lbl_usb, "USB Card Reader Mode");
    lv_obj_set_style_text_color(lbl_usb, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(lbl_usb, LV_ALIGN_LEFT_MID, 5, 0);

    lv_obj_t *btn_usb = lv_btn_create(cont_usb);
    lv_obj_set_size(btn_usb, 80, 30);
    lv_obj_align(btn_usb, LV_ALIGN_RIGHT_MID, -5, 0);
    lv_obj_set_style_bg_color(btn_usb, lv_color_hex(0x00AEEF), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_usb, btn_usb_mode_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_btn_usb = lv_label_create(btn_usb);
    lv_label_set_text(lbl_btn_usb, "ENTER");
    lv_obj_center(lbl_btn_usb);
}