#pragma once

#include <lvgl.h>
#include <stdio.h>
#include "GPS_Driver.hpp"
#include "IMU_Driver.hpp"
#include "DragRace_Manager.hpp"

// 引用外部
extern void screen_gesture_event_cb(lv_event_t *e);
extern GPS_Driver gps;
extern IMU_Driver imu;

// --- 全局对象 ---
lv_obj_t *ui_ScreenDrag = NULL;
lv_obj_t *ui_PanelStatus = NULL;  // 中央大框
lv_obj_t *ui_LblDragState = NULL; // 顶部状态字 "READY"
lv_obj_t *ui_LblDragTimer = NULL; // 中间大时间 "0.00"
lv_obj_t *ui_LblDragUnit = NULL;  // 单位 "s"
lv_obj_t *ui_LblLiveSpeed = NULL; // 底部速度
lv_obj_t *ui_BarLiveG = NULL;     // 底部G值条

lv_timer_t *timer_drag_refresh = NULL;
DragRaceManager dragMgr;

// --- 样式定义 ---
void set_drag_style(lv_color_t color, const char *state_text)
{
    if (ui_PanelStatus)
    {
        lv_obj_set_style_border_color(ui_PanelStatus, color, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(ui_PanelStatus, color, LV_PART_MAIN);
    }
    if (ui_LblDragState)
    {
        lv_label_set_text(ui_LblDragState, state_text);
        lv_obj_set_style_text_color(ui_LblDragState, color, LV_PART_MAIN);
    }
    if (ui_LblDragTimer)
    {
        lv_obj_set_style_text_color(ui_LblDragTimer, color, LV_PART_MAIN);
    }
}

// --- 刷新回调 (20ms) ---
void drag_timer_cb(lv_timer_t *timer)
{
    if (lv_scr_act() != ui_ScreenDrag)
        return;

    // 1. 获取数据
    float spd = gps.getSpeed();
    if (spd < 0)
        spd = 0;
    float g_val = imu.lon_g;

    dragMgr.update(spd, g_val);
    DragState state = dragMgr.getState();

    // 2. 状态机视觉切换
    static DragState lastState = DRAG_IDLE;
    // 强制每帧检查颜色，防止初始化时颜色不对
    if (state != lastState || true)
    {
        switch (state)
        {
        case DRAG_READY:
            set_drag_style(lv_color_hex(0x00FF00), "READY TO LAUNCH"); // 绿
            lv_obj_set_style_shadow_width(ui_PanelStatus, 20, LV_PART_MAIN);
            break;
        case DRAG_RUNNING:
            set_drag_style(lv_color_hex(0xFFFFFF), "GO !!!"); // 白
            lv_obj_set_style_shadow_width(ui_PanelStatus, 0, LV_PART_MAIN);
            break;
        case DRAG_FINISHED:
            set_drag_style(lv_color_hex(0xFFD700), "RESULT"); // 金
            lv_obj_set_style_shadow_width(ui_PanelStatus, 40, LV_PART_MAIN);
            break;
        default:                                                    // IDLE
            set_drag_style(lv_color_hex(0x555555), "STOP & RESET"); // 灰
            lv_obj_set_style_shadow_width(ui_PanelStatus, 0, LV_PART_MAIN);
            break;
        }
        lastState = state;
    }

    // 3. 更新时间 (大数字) - [核心修复] 使用 String 类
    float timeVal = dragMgr.getCurrentTime();

    // 如果是 IDLE 状态，强制显示 0.00
    if (state == DRAG_IDLE || state == DRAG_READY)
        timeVal = 0.00;

    // 使用 Arduino String 转换，彻底避开 snprintf 浮点问题
    String timeStr = String(timeVal, 2);
    lv_label_set_text(ui_LblDragTimer, timeStr.c_str());

    // 4. 更新底部速度 - [核心修复] 使用 String 类
    String spdStr = String(spd, 1) + " km/h";
    lv_label_set_text(ui_LblLiveSpeed, spdStr.c_str());

    // 5. 更新 G 值条
    int barVal = (int)(abs(g_val) * 100);
    if (barVal > 100)
        barVal = 100;
    lv_bar_set_value(ui_BarLiveG, barVal, LV_ANIM_OFF);

    // G值变色
    if (barVal > 40)
        lv_obj_set_style_bg_color(ui_BarLiveG, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
    else
        lv_obj_set_style_bg_color(ui_BarLiveG, lv_color_hex(0x00AEEF), LV_PART_INDICATOR);
}

// --- 页面构建 ---
void build_drag_page()
{
    if (ui_ScreenDrag)
    {
        if (!timer_drag_refresh)
            timer_drag_refresh = lv_timer_create(drag_timer_cb, 20, NULL);
        return;
    }

    ui_ScreenDrag = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenDrag, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_add_event_cb(ui_ScreenDrag, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    // 1. 中央大面板
    ui_PanelStatus = lv_obj_create(ui_ScreenDrag);
    lv_obj_set_size(ui_PanelStatus, 260, 160);
    lv_obj_align(ui_PanelStatus, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_opa(ui_PanelStatus, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui_PanelStatus, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui_PanelStatus, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_radius(ui_PanelStatus, 12, LV_PART_MAIN);
    lv_obj_clear_flag(ui_PanelStatus, LV_OBJ_FLAG_SCROLLABLE);

    // 2. 状态文字 (顶部)
    ui_LblDragState = lv_label_create(ui_PanelStatus);
    lv_label_set_text(ui_LblDragState, "STOP TO RESET");
    lv_obj_set_style_text_font(ui_LblDragState, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(ui_LblDragState, LV_ALIGN_TOP_MID, 0, 10);

    // 3. 核心计时器 (调试模式：不放大，只用20号字)
    ui_LblDragTimer = lv_label_create(ui_PanelStatus);
    // 先设置一个默认值
    lv_label_set_text(ui_LblDragTimer, "0.00");

    // [修改] 只用标准 20 号字体，不加 zoom
    lv_obj_set_style_text_font(ui_LblDragTimer, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LblDragTimer, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_align(ui_LblDragTimer, LV_ALIGN_CENTER, 0, 0);

    // 4. 单位 s
    ui_LblDragUnit = lv_label_create(ui_PanelStatus);
    lv_label_set_text(ui_LblDragUnit, "s");
    lv_obj_set_style_text_font(ui_LblDragUnit, &lv_font_montserrat_14, LV_PART_MAIN); // 用更小的14号区分
    lv_obj_set_style_text_color(ui_LblDragUnit, lv_color_hex(0x888888), LV_PART_MAIN);
    // 因为字变小了，单位紧跟在数字后面
    lv_obj_align_to(ui_LblDragUnit, ui_LblDragTimer, LV_ALIGN_OUT_RIGHT_BOTTOM, 5, 0);

    // 5. 底部信息栏

    // 速度显示
    ui_LblLiveSpeed = lv_label_create(ui_ScreenDrag);
    lv_label_set_text(ui_LblLiveSpeed, "0.0 km/h");
    lv_obj_set_style_text_font(ui_LblLiveSpeed, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LblLiveSpeed, lv_color_hex(0x00AEEF), LV_PART_MAIN);
    lv_obj_align(ui_LblLiveSpeed, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    // G值条
    ui_BarLiveG = lv_bar_create(ui_ScreenDrag);
    lv_obj_set_size(ui_BarLiveG, 80, 10);
    lv_obj_align(ui_BarLiveG, LV_ALIGN_BOTTOM_RIGHT, -10, -15);
    lv_bar_set_range(ui_BarLiveG, 0, 100);
    lv_obj_set_style_bg_color(ui_BarLiveG, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_BarLiveG, lv_color_hex(0x00AEEF), LV_PART_INDICATOR);

    lv_obj_t *lbl_g = lv_label_create(ui_ScreenDrag);
    lv_label_set_text(lbl_g, "G");
    lv_obj_set_style_text_font(lbl_g, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_g, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_align_to(lbl_g, ui_BarLiveG, LV_ALIGN_OUT_LEFT_MID, -5, 0);

    timer_drag_refresh = lv_timer_create(drag_timer_cb, 20, NULL);
}