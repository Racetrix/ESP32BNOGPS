#pragma once

#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include "System_Config.hpp"
#include "GPS_Driver.hpp"
#include "Track_Manager.hpp"

extern GPS_Driver gps;
extern TrackManager trackMgr;
extern lv_obj_t *ui_ScreenMain;

// --- 全局 UI 对象 ---
lv_obj_t *ui_ScreenMode = NULL;
lv_obj_t *ui_BtnRoam;
lv_obj_t *ui_LabelRoamStatus;

lv_obj_t *ui_BtnTrack;
lv_obj_t *ui_LabelTrackStatus;
lv_obj_t *ui_LabelTrackInfo;

// --- 赛道等待页面对象 ---
lv_obj_t *ui_ScreenTrackWait = NULL;
lv_obj_t *ui_LabelWaitTitle;
lv_obj_t *ui_LabelWaitCoords;
lv_obj_t *ui_LabelWaitDist;
lv_obj_t *ui_BtnWaitExit;
lv_timer_t *timer_track_wait = NULL;

// --- 前向声明 ---
extern void screen_gesture_event_cb(lv_event_t *e);
void build_track_wait_page();
void build_mode_page();

// 辅助函数: 计算两点距离
float calc_dist_ui(double lat1, double lon1, double lat2, double lon2)
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

// ==========================================
// 事件回调
// ==========================================

// 1. 漫游模式点击
void btn_roam_event_cb(lv_event_t *e)
{
    if (!gps.tgps.location.isValid() && !sys_cfg.is_running)
        return;

    if (!sys_cfg.is_running)
    {
        sys_cfg.current_mode = MODE_ROAM;
        sys_cfg.is_running = true;
        sys_cfg.session_start_ms = millis();
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x00FF00), LV_PART_MAIN);
        lv_label_set_text(ui_LabelRoamStatus, "RUNNING");
        // 不删除旧屏幕，快速切换
        lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
    }
    else
    {
        sys_cfg.is_running = false;
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x00AEEF), LV_PART_MAIN);
        lv_label_set_text(ui_LabelRoamStatus, "READY");
    }
}

// 2. 赛道模式点击
// 2. 赛道模式点击
// 2. 赛道模式点击
void btn_track_event_cb(lv_event_t *e)
{
    // 如果没有配置赛道，直接忽略
    if (!trackMgr.isTrackSetup())
        return;

    // [新增逻辑] 如果已经是激活状态 (Armed)，再次点击则是“停止/退出”
    if (trackMgr.isArmed())
    {
        Serial.println("[UI] Track Button -> STOP RACE");

        // 1. 退出赛道逻辑
        trackMgr.exitTrackMode();

        // 2. 停止系统运行状态 (停止录制日志等)
        sys_cfg.is_running = false;

        // 3. 视觉反馈交给 timer 去更新 (变回灰色)
        return;
    }

    // [原有逻辑] 如果是未激活状态，则进入“等待发车”
    // 互斥检查：如果正在漫游，不允许进入
    if (sys_cfg.is_running && sys_cfg.current_mode == MODE_ROAM)
        return;

    trackMgr.enterStandbyMode();
    sys_cfg.current_mode = MODE_TRACK;

    build_track_wait_page();
    lv_scr_load_anim(ui_ScreenTrackWait, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}
// 3. [修复] 退出等待页面 - 强制安全逻辑
// void btn_wait_exit_event_cb(lv_event_t *e)
// {
//     Serial.println("[UI] Cancel Clicked");

//     // A. 绝对优先：立刻杀死定时器
//     if (timer_track_wait)
//     {
//         lv_timer_del(timer_track_wait);
//         timer_track_wait = NULL;
//     }

//     // B. [核心修复] 强制清理 ui_ScreenMode 指针
//     // 为了防止野指针崩溃，我们假设 ui_ScreenMode 可能已经损坏
//     // 手动置空，迫使 build_mode_page 重新检查或创建
//     // 注意：如果 ui_ScreenMode 是有效的，LVGL 会在内存里有两个一样的屏幕吗？
//     // 为了安全起见，我们先检查有效性。如果不想太复杂，直接用下面的逻辑：

//     if (ui_ScreenMode != NULL)
//     {
//         // 如果我们不确定它是否是僵尸对象，最稳妥的是不碰它，或者相信它存在。
//         // 但既然崩了，我们就重新构建。
//         if (!lv_obj_is_valid(ui_ScreenMode))
//         {
//             ui_ScreenMode = NULL; // 如果 LVGL 认为它无效，置空
//         }
//     }

//     if (!ui_ScreenMode)
//     {
//         Serial.println("[UI] Rebuilding Mode Page");
//         build_mode_page();
//     }

//     // C. 切换页面
//     // auto_del = true: 切换完成后，自动销毁当前的 ui_ScreenTrackWait
//     lv_scr_load_anim(ui_ScreenMode, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, true);

//     // D. 指针置空 (配合 auto_del = true)
//     ui_ScreenTrackWait = NULL;
// }

// 3. 退出等待页面
void btn_wait_exit_event_cb(lv_event_t *e)
{
    Serial.println("[UI] Cancel Clicked");

    // [核心修改] 用户退出了，必须关闭赛道检测！
    // 否则回到主页后，如果车经过起点，依然会在后台触发计时
    trackMgr.exitTrackMode();

    // A. 杀死定时器
    if (timer_track_wait)
    {
        lv_timer_del(timer_track_wait);
        timer_track_wait = NULL;
    }

    // ... (原本的页面切换代码保持不变) ...
    if (ui_ScreenMode != NULL && !lv_obj_is_valid(ui_ScreenMode))
    {
        ui_ScreenMode = NULL;
    }
    if (!ui_ScreenMode)
        build_mode_page();

    lv_scr_load_anim(ui_ScreenMode, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, true);
    ui_ScreenTrackWait = NULL;
}

// ==========================================
// 定时器逻辑
// ==========================================

void mode_timer_cb(lv_timer_t *timer)
{
    if (!ui_ScreenMode || !lv_obj_is_valid(ui_ScreenMode))
        return;
    if (lv_scr_act() != ui_ScreenMode)
        return;

    bool is_track_active = trackMgr.isArmed();                                        // 赛道模式是否激活
    bool is_roam_running = (sys_cfg.is_running && sys_cfg.current_mode == MODE_ROAM); // 漫游是否在跑

    // --- 1. 更新 ROAM 按钮状态 ---
    // 如果赛道模式激活了，禁用漫游按钮
    if (is_track_active)
    {
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x333333), LV_PART_MAIN); // 变灰
        lv_label_set_text(ui_LabelRoamStatus, "DISABLED");
        lv_obj_clear_flag(ui_BtnRoam, LV_OBJ_FLAG_CLICKABLE);   // 禁止点击
        lv_obj_set_style_bg_opa(ui_BtnRoam, 100, LV_PART_MAIN); // 变暗
    }
    else if (is_roam_running)
    {
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x00FF00), LV_PART_MAIN); // 绿色 (运行中)
        lv_label_set_text(ui_LabelRoamStatus, "RUNNING");
        lv_obj_add_flag(ui_BtnRoam, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(ui_BtnRoam, 255, LV_PART_MAIN);
    }
    else if (gps.tgps.location.isValid())
    {
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x00AEEF), LV_PART_MAIN); // 蓝色 (就绪)
        lv_label_set_text(ui_LabelRoamStatus, "READY");
        lv_obj_add_flag(ui_BtnRoam, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(ui_BtnRoam, 255, LV_PART_MAIN);
    }
    else
    {
        // GPS 未定位
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_label_set_text_fmt(ui_LabelRoamStatus, "WAIT GPS (%d)", gps.getSatellites());
        lv_obj_clear_flag(ui_BtnRoam, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(ui_BtnRoam, 150, LV_PART_MAIN);
    }

    // --- 2. 更新 TRACK 按钮状态 ---
    // 如果漫游在跑，禁用赛道按钮
    if (is_roam_running)
    {
        lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0x222222), LV_PART_MAIN);
        lv_label_set_text(ui_LabelTrackStatus, "ROAMING BUSY");
        lv_obj_clear_flag(ui_BtnTrack, LV_OBJ_FLAG_CLICKABLE);
    }
    else if (trackMgr.isTrackSetup())
    {
        // [新增] 如果正在跑比赛 (Armed)，显示红色停止按钮
        if (is_track_active)
        {
            lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0xFF0000), LV_PART_MAIN); // 红色
            lv_label_set_text(ui_LabelTrackStatus, "STOP RACE");
            lv_label_set_text(ui_LabelTrackInfo, "Running...");
        }
        else
        {
            // 就绪状态
            lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0x9C27B0), LV_PART_MAIN); // 紫色
            lv_label_set_text(ui_LabelTrackStatus, "START RACE");
            int type = trackMgr.getCurrentTrackType();
            lv_label_set_text(ui_LabelTrackInfo, (type == 0) ? "CIRCUIT" : "SPRINT");
        }
        lv_obj_add_flag(ui_BtnTrack, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(ui_BtnTrack, 255, LV_PART_MAIN);
    }
    else
    {
        // 无赛道数据
        lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0x222222), LV_PART_MAIN);
        lv_label_set_text(ui_LabelTrackStatus, "SETUP VIA APP");
        lv_label_set_text(ui_LabelTrackInfo, "NO DATA");
        lv_obj_clear_flag(ui_BtnTrack, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(ui_BtnTrack, 150, LV_PART_MAIN);
    }
}

void track_wait_timer_cb(lv_timer_t *timer)
{
    // [安全检查]
    if (!ui_ScreenTrackWait || !lv_obj_is_valid(ui_ScreenTrackWait))
        return;
    if (lv_scr_act() != ui_ScreenTrackWait)
        return;

    if (sys_cfg.is_running)
    {
        if (timer_track_wait)
        {
            lv_timer_del(timer_track_wait);
            timer_track_wait = NULL;
        }
        lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
        return;
    }

    // 更新距离 (带 Age 检查，防止旧数据)
    if (gps.tgps.location.isValid() && gps.tgps.location.age() < 3000)
    {
        double sLat, sLon;
        trackMgr.getStartPoint(sLat, sLon);

        if (abs(sLat) < 0.1)
            return;

        float dist = calc_dist_ui(gps.tgps.location.lat(), gps.tgps.location.lng(), sLat, sLon);

        if (dist < trackMgr.getTriggerRadius() * 2)
        {
            lv_obj_set_style_text_color(ui_LabelWaitDist, lv_color_hex(0x00FF00), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_text_color(ui_LabelWaitDist, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        }

        char buf[32];
        snprintf(buf, sizeof(buf), "DIST: %.1f m", dist);
        lv_label_set_text(ui_LabelWaitDist, buf);
    }
    else
    {
        lv_label_set_text(ui_LabelWaitDist, "DIST: GPS LOST");
        lv_obj_set_style_text_color(ui_LabelWaitDist, lv_color_hex(0x888888), LV_PART_MAIN);
    }
}

// ==========================================
// 页面构建
// ==========================================

void build_track_wait_page()
{
    if (ui_ScreenTrackWait && lv_obj_is_valid(ui_ScreenTrackWait))
    {
        if (!timer_track_wait)
            timer_track_wait = lv_timer_create(track_wait_timer_cb, 100, NULL);
        return;
    }

    ui_ScreenTrackWait = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenTrackWait, lv_color_hex(0x000000), LV_PART_MAIN);

    // 1. 标题 (Y=15)
    ui_LabelWaitTitle = lv_label_create(ui_ScreenTrackWait);
    lv_label_set_text(ui_LabelWaitTitle, "WAITING FOR START");
    lv_obj_set_style_text_font(ui_LabelWaitTitle, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelWaitTitle, lv_color_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_align(ui_LabelWaitTitle, LV_ALIGN_TOP_MID, 0, 15);

    // 2. 坐标 (Y=45)
    ui_LabelWaitCoords = lv_label_create(ui_ScreenTrackWait);
    double sLat, sLon;
    trackMgr.getStartPoint(sLat, sLon);
    char coord_buf[64];
    if (abs(sLat) > 0.1)
    {
        snprintf(coord_buf, sizeof(coord_buf), "START: %.5f, %.5f", sLat, sLon);
    }
    else
    {
        snprintf(coord_buf, sizeof(coord_buf), "START: --.----, --.----");
    }
    lv_label_set_text(ui_LabelWaitCoords, coord_buf);
    lv_obj_set_style_text_font(ui_LabelWaitCoords, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelWaitCoords, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(ui_LabelWaitCoords, LV_ALIGN_TOP_MID, 0, 45);

    // 3. 距离 (上移到 -45，给中间腾位置)
    ui_LabelWaitDist = lv_label_create(ui_ScreenTrackWait);
    lv_label_set_text(ui_LabelWaitDist, "DIST: --.- m");
    lv_obj_set_style_text_font(ui_LabelWaitDist, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_align(ui_LabelWaitDist, LV_ALIGN_CENTER, 0, -45);

    // 4. 提示 (正中间偏下，Y=10)
    lv_obj_t *hint = lv_label_create(ui_ScreenTrackWait);
    lv_label_set_text(hint, "Drive through start line\nto automatically begin.");
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 10);

    // 5. 按钮 (贴底，Y=-15，改小一点)
    ui_BtnWaitExit = lv_btn_create(ui_ScreenTrackWait);
    lv_obj_set_size(ui_BtnWaitExit, 100, 36); // 变小
    lv_obj_align(ui_BtnWaitExit, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_set_style_bg_color(ui_BtnWaitExit, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_radius(ui_BtnWaitExit, 18, LV_PART_MAIN);
    lv_obj_add_event_cb(ui_BtnWaitExit, btn_wait_exit_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_exit = lv_label_create(ui_BtnWaitExit);
    lv_label_set_text(lbl_exit, "CANCEL");
    lv_obj_set_style_text_font(lbl_exit, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_center(lbl_exit);

    timer_track_wait = lv_timer_create(track_wait_timer_cb, 100, NULL);
}

void build_mode_page()
{
    if (ui_ScreenMode && lv_obj_is_valid(ui_ScreenMode))
        return;

    ui_ScreenMode = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenMode, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_add_event_cb(ui_ScreenMode, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *title = lv_label_create(ui_ScreenMode);
    lv_label_set_text(title, "SELECT MODE");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // 1. ROAMING
    ui_BtnRoam = lv_btn_create(ui_ScreenMode);
    lv_obj_set_size(ui_BtnRoam, 280, 80);
    lv_obj_align(ui_BtnRoam, LV_ALIGN_TOP_MID, 0, 50);
    // [修改 1] 将 CLICKED 改为 LONG_PRESSED (防误触)
    lv_obj_add_event_cb(ui_BtnRoam, btn_roam_event_cb, LV_EVENT_LONG_PRESSED, NULL);

    // [修改 2] 让按钮也能识别滑动手势 (解决在按钮上没法滑回主页的问题)
    lv_obj_add_event_cb(ui_BtnRoam, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *lbl_roam = lv_label_create(ui_BtnRoam);
    lv_label_set_text(lbl_roam, "ROAMING");
    lv_obj_set_style_text_font(lbl_roam, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_align(lbl_roam, LV_ALIGN_TOP_LEFT, 10, 10);

    ui_LabelRoamStatus = lv_label_create(ui_BtnRoam);
    lv_label_set_text(ui_LabelRoamStatus, "CHECKING GPS...");
    lv_obj_set_style_text_font(ui_LabelRoamStatus, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(ui_LabelRoamStatus, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    // 2. TRACK MODE
    ui_BtnTrack = lv_btn_create(ui_ScreenMode);
    lv_obj_set_size(ui_BtnTrack, 280, 80);
    lv_obj_align(ui_BtnTrack, LV_ALIGN_TOP_MID, 0, 145);
    lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0x222222), LV_PART_MAIN);
    // [修改 1] 改为长按
    lv_obj_add_event_cb(ui_BtnTrack, btn_track_event_cb, LV_EVENT_LONG_PRESSED, NULL);

    // [修改 2] 添加手势支持
    lv_obj_add_event_cb(ui_BtnTrack, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *lbl_track = lv_label_create(ui_BtnTrack);
    lv_label_set_text(lbl_track, "TRACK MODE");
    lv_obj_set_style_text_font(lbl_track, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_track, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(lbl_track, LV_ALIGN_TOP_LEFT, 10, 10);

    ui_LabelTrackStatus = lv_label_create(ui_BtnTrack);
    lv_label_set_text(ui_LabelTrackStatus, "LOADING...");
    lv_obj_set_style_text_font(ui_LabelTrackStatus, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(ui_LabelTrackStatus, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    ui_LabelTrackInfo = lv_label_create(ui_BtnTrack);
    lv_label_set_text(ui_LabelTrackInfo, "-");
    lv_obj_set_style_text_font(ui_LabelTrackInfo, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelTrackInfo, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(ui_LabelTrackInfo, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    lv_timer_create(mode_timer_cb, 500, NULL);
}