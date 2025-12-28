#pragma once

#include <lvgl.h>
#include "System_Config.hpp"
#include "GPS_Driver.hpp"

// 引用外部对象
extern GPS_Driver gps;
extern lv_obj_t *ui_ScreenMain;

// 本页全局对象
lv_obj_t *ui_ScreenMode;
lv_obj_t *ui_BtnRoam;
lv_obj_t *ui_LabelRoamStatus;
lv_obj_t *ui_BtnTrack;

// --- 新增这一行：前向声明手势回调函数 ---
extern void screen_gesture_event_cb(lv_event_t *e);

// --- 事件回调 ---

// 漫游模式点击事件
// 漫游模式点击事件
void btn_roam_event_cb(lv_event_t *e)
{
    if (gps.tgps.location.isValid() || sys_cfg.is_running)
    {
        if (!sys_cfg.is_running)
        {
            // 开始
            sys_cfg.current_mode = MODE_ROAM;
            sys_cfg.is_running = true;
            sys_cfg.session_start_ms = millis(); // 记录开始时间
            lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x00FF00), LV_PART_MAIN);
            lv_label_set_text(ui_LabelRoamStatus, "RUNNING");

            // [修改] 瞬间跳回仪表盘 (无动画)
            lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
        }
        else
        {
            // 停止
            sys_cfg.is_running = false;
            lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x00AEEF), LV_PART_MAIN);
            lv_label_set_text(ui_LabelRoamStatus, "READY");
        }
    }
}

// 赛道模式点击 (暂未开发)
void btn_track_event_cb(lv_event_t *e)
{
    // 只是为了演示，不做实际逻辑
}

// 定时器：检查 GPS 状态并更新 UI
void mode_timer_cb(lv_timer_t *timer)
{
    if (lv_scr_act() != ui_ScreenMode)
        return; // 不在前台就不更新

    // 如果已经在运行，就不检查 GPS 锁定状态了，保持运行态
    if (sys_cfg.is_running)
        return;

    // --- 漫游模式状态逻辑 ---
    if (gps.tgps.location.isValid())
    {
        // 3D 定位成功
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x00AEEF), LV_PART_MAIN); // 蓝色 (就绪)
        lv_label_set_text(ui_LabelRoamStatus, "READY TO START");
        lv_obj_add_flag(ui_BtnRoam, LV_OBJ_FLAG_CLICKABLE); // 允许点击
        lv_obj_set_style_bg_opa(ui_BtnRoam, 255, LV_PART_MAIN);
    }
    else
    {
        // 无定位
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x333333), LV_PART_MAIN); // 灰色
        lv_label_set_text_fmt(ui_LabelRoamStatus, "WAITING GPS (%d)", gps.getSatellites());
        lv_obj_clear_flag(ui_BtnRoam, LV_OBJ_FLAG_CLICKABLE);   // 禁止点击
        lv_obj_set_style_bg_opa(ui_BtnRoam, 150, LV_PART_MAIN); // 半透明
    }
}

// --- 构建页面 ---

// 返回按钮事件
// void btn_settings_back_event_cb(lv_event_t *e)
// {
//     // 因为设置页在左边，返回主页时，应该向左滑出 (Move Left)
//     lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
// }
void build_mode_page()
{
    if (ui_ScreenMode)
        return;

    ui_ScreenMode = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenMode, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_add_event_cb(ui_ScreenMode, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    // 标题
    lv_obj_t *title = lv_label_create(ui_ScreenMode);
    lv_label_set_text(title, "SELECT MODE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // === 1. 漫游模式按钮 (大卡片) ===
    ui_BtnRoam = lv_btn_create(ui_ScreenMode);
    lv_obj_set_size(ui_BtnRoam, 280, 80);
    lv_obj_align(ui_BtnRoam, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_add_event_cb(ui_BtnRoam, btn_roam_event_cb, LV_EVENT_CLICKED, NULL);

    // 图标/大字
    lv_obj_t *lbl_roam = lv_label_create(ui_BtnRoam);
    lv_label_set_text(lbl_roam, "ROAMING");
    lv_obj_set_style_text_font(lbl_roam, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(lbl_roam, LV_ALIGN_TOP_LEFT, 10, 10);

    // 状态文字
    ui_LabelRoamStatus = lv_label_create(ui_BtnRoam);
    lv_label_set_text(ui_LabelRoamStatus, "CHECKING GPS...");
    lv_obj_set_style_text_font(ui_LabelRoamStatus, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(ui_LabelRoamStatus, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    // === 2. 赛道模式按钮 (暂时禁用) ===
    ui_BtnTrack = lv_btn_create(ui_ScreenMode);
    lv_obj_set_size(ui_BtnTrack, 280, 80);
    lv_obj_align(ui_BtnTrack, LV_ALIGN_TOP_MID, 0, 145);
    lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0x222222), LV_PART_MAIN); // 深灰
    lv_obj_add_event_cb(ui_BtnTrack, btn_track_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_track = lv_label_create(ui_BtnTrack);
    lv_label_set_text(lbl_track, "TRACK MODE");
    lv_obj_set_style_text_font(lbl_track, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_track, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_align(lbl_track, LV_ALIGN_TOP_LEFT, 10, 10);

    lv_obj_t *lbl_track_hint = lv_label_create(ui_BtnTrack);
    lv_label_set_text(lbl_track_hint, "COMING SOON");
    lv_obj_set_style_text_font(lbl_track_hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_track_hint, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_align(lbl_track_hint, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    // 创建一个定时器，每 500ms 检查一次 GPS 状态
    lv_timer_create(mode_timer_cb, 500, NULL);
}