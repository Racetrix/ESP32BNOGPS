#pragma once

#include <lvgl.h>
#include "System_Config.hpp"
// 移除 GPS_Driver.hpp 的引用，因为这里不需要操作硬件了

extern void screen_gesture_event_cb(lv_event_t *e);
extern lv_obj_t *ui_ScreenMain;

// 全局对象
lv_obj_t *ui_ScreenSettings;

// --- 事件回调 ---

// 蓝牙开关回调
void sw_bt_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    sys_cfg.bluetooth_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sys_cfg.save();

    if (sys_cfg.bluetooth_on)
        Serial.println("BT: ON");
    else
        Serial.println("BT: OFF");
}

// GPS 频率回调 (回滚：只改配置，不发指令)
void sw_gps_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    sys_cfg.gps_10hz_mode = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sys_cfg.save();
    Serial.printf("GPS Mode Config: %s (Software Only)\n", sys_cfg.gps_10hz_mode ? "10Hz" : "1Hz");
}

// IMU 交换轴回调
void sw_imu_swap_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    sys_cfg.imu_swap_axis = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sys_cfg.save();
}

// IMU 反向 X 回调
void sw_imu_inv_x_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    sys_cfg.imu_invert_x = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sys_cfg.save();
}

// IMU 反向 Y 回调
void sw_imu_inv_y_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    sys_cfg.imu_invert_y = lv_obj_has_state(sw, LV_STATE_CHECKED);
    sys_cfg.save();
}

// 返回按钮事件
void btn_settings_back_event_cb(lv_event_t *e)
{
    lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

// 辅助函数：创建设置行
void create_setting_item(lv_obj_t *parent, const char *text, bool current_state, lv_event_cb_t cb)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 300, 50);

    // [优化] 背景设为纯色 (深灰)，不透明
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, LV_PART_MAIN); // 强制不透明

    lv_obj_set_style_border_width(cont, 0, LV_PART_MAIN);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

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

void btn_usb_mode_event_cb(lv_event_t *e)
{
    // 设置标志位
    sys_cfg.boot_into_usb = true;
    sys_cfg.save();

    // 提示并重启
    // 这里简单直接重启，因为 USB 模式需要干净的环境
    Serial.println("Rebooting into USB Mode...");
    ESP.restart();
}

// --- 构建设置页面 ---
void build_settings_page()
{
    if (ui_ScreenSettings)
        return;

    ui_ScreenSettings = lv_obj_create(NULL);
    // [优化] 页面背景纯黑，不透明
    lv_obj_set_style_bg_color(ui_ScreenSettings, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui_ScreenSettings, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_add_event_cb(ui_ScreenSettings, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    // 顶部标题栏
    lv_obj_t *header = lv_obj_create(ui_ScreenSettings);
    lv_obj_set_size(header, 320, 40);
    // [优化] 标题栏纯色
    lv_obj_set_style_bg_color(header, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(header, 0, LV_PART_MAIN);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "SETTINGS");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00AEEF), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 5, 0);

    // 返回按钮
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

    // [优化] 关键点：列表容器背景设为纯黑，并且不透明
    // 之前这里是 bg_opa = 0 (透明)，这是卡顿的元凶
    lv_obj_set_style_bg_color(list_cont, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(list_cont, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_border_width(list_cont, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(list_cont, 10, LV_PART_MAIN);

    // 添加设置项
    create_setting_item(list_cont, "Bluetooth", sys_cfg.bluetooth_on, sw_bt_event_cb);
    create_setting_item(list_cont, "GPS 10Hz Mode (Log)", sys_cfg.gps_10hz_mode, sw_gps_event_cb);
    create_setting_item(list_cont, "Swap G-Ball Axis (X/Y)", sys_cfg.imu_swap_axis, sw_imu_swap_event_cb);
    create_setting_item(list_cont, "Invert G-Ball X", sys_cfg.imu_invert_x, sw_imu_inv_x_event_cb);
    create_setting_item(list_cont, "Invert G-Ball Y", sys_cfg.imu_invert_y, sw_imu_inv_y_event_cb);

    // USB 模式按钮 (同样优化样式)
    lv_obj_t *cont_usb = lv_obj_create(list_cont);
    lv_obj_set_size(cont_usb, 300, 50);
    lv_obj_set_style_bg_color(cont_usb, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(cont_usb, LV_OPA_COVER, LV_PART_MAIN); // 不透明
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