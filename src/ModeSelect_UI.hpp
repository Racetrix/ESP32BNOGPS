#pragma once

#include <lvgl.h>
#include <math.h>
#include <stdio.h>
#include "System_Config.hpp"
#include "GPS_Driver.hpp"
#include "Track_Manager.hpp"


LV_FONT_DECLARE(font_race);    // 14px (小号，用于标题、状态、详情)
LV_FONT_DECLARE(font_race_30); // 30px (大号，用于按钮核心文字)


static lv_style_t style_text_30; // 30号 大字样式
static lv_style_t style_text_14; // 14号 小字样式
static bool styles_initialized = false;


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

// 辅助函数：统一初始化样式
// 只需要在每个 build 函数的开头调用一次即可
// 辅助函数：统一初始化样式
static void ensure_styles_init()
{
    if (styles_initialized)
        return;

    lv_style_init(&style_text_30);
    lv_style_set_text_font(&style_text_30, &font_race_30);
    lv_style_set_text_color(&style_text_30, lv_color_white());

    lv_style_init(&style_text_14);
    lv_style_set_text_font(&style_text_14, &font_race);
    lv_style_set_text_color(&style_text_14, lv_color_white());

    styles_initialized = true;
}
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

// 确保已声明字体
LV_FONT_DECLARE(font_race);

// 如果你在文件顶部已经定义了 style_text_14，这里就不需要重复定义
// 如果没有，你需要确保在使用前应用样式的代码是有效的
// 建议：直接复用 build_mode_page 里定义好的 style_text_14

void mode_timer_cb(lv_timer_t *timer)
{
    if (!ui_ScreenMode || !lv_obj_is_valid(ui_ScreenMode))
        return;
    if (lv_scr_act() != ui_ScreenMode)
        return;

    bool is_track_active = trackMgr.isArmed();                                        // 赛道模式是否激活
    bool is_roam_running = (sys_cfg.is_running && sys_cfg.current_mode == MODE_ROAM); // 漫游是否在跑

    // --- 1. 更新 街道漫游 (ROAM) 按钮状态 ---
    if (is_track_active)
    {
        // [禁用] 因为正在跑赛道
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x333333), LV_PART_MAIN); // 变灰
        lv_label_set_text(ui_LabelRoamStatus, "已禁用");                             // DISABLED
        lv_obj_clear_flag(ui_BtnRoam, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(ui_BtnRoam, 100, LV_PART_MAIN);
    }
    else if (is_roam_running)
    {
        // [运行中]
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x00FF00), LV_PART_MAIN); // 绿色
        lv_label_set_text(ui_LabelRoamStatus, "运行中");                             // RUNNING
        lv_obj_add_flag(ui_BtnRoam, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(ui_BtnRoam, 255, LV_PART_MAIN);
    }
    else if (gps.tgps.location.isValid())
    {
        // [就绪] GPS 有定位
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x00AEEF), LV_PART_MAIN); // 蓝色
        lv_label_set_text(ui_LabelRoamStatus, "就绪");                               // READY
        lv_obj_add_flag(ui_BtnRoam, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(ui_BtnRoam, 255, LV_PART_MAIN);
    }
    else
    {
        // [等待GPS]
        lv_obj_set_style_bg_color(ui_BtnRoam, lv_color_hex(0x333333), LV_PART_MAIN);
        // 使用 fmt 格式化字符串
        lv_label_set_text_fmt(ui_LabelRoamStatus, "等待卫星 (%d)", gps.getSatellites()); // WAIT GPS
        lv_obj_clear_flag(ui_BtnRoam, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(ui_BtnRoam, 150, LV_PART_MAIN);
    }

    // --- 2. 更新 赛道模式 (TRACK) 按钮状态 ---
    if (is_roam_running)
    {
        // [禁用] 因为正在跑漫游
        lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0x222222), LV_PART_MAIN);
        lv_label_set_text(ui_LabelTrackStatus, "漫游占用"); // ROAMING BUSY -> 漫游占用
        lv_obj_clear_flag(ui_BtnTrack, LV_OBJ_FLAG_CLICKABLE);
    }
    else if (trackMgr.isTrackSetup())
    {
        if (is_track_active)
        {
            // [正在比赛] -> 显示停止
            lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0xFF0000), LV_PART_MAIN); // 红色
            lv_label_set_text(ui_LabelTrackStatus, "停止比赛");                           // STOP RACE
            lv_label_set_text(ui_LabelTrackInfo, "比赛进行中");                           // Running...
        }
        else
        {
            // [就绪] -> 显示开始
            lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0x9C27B0), LV_PART_MAIN); // 紫色
            lv_label_set_text(ui_LabelTrackStatus, "开始比赛");                           // START RACE

            // 显示赛道类型
            int type = trackMgr.getCurrentTrackType();
            if (type == 0)
                lv_label_set_text(ui_LabelTrackInfo, "圈速"); // CIRCUIT
            else
                lv_label_set_text(ui_LabelTrackInfo, "冲刺"); // SPRINT
        }
        lv_obj_add_flag(ui_BtnTrack, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(ui_BtnTrack, 255, LV_PART_MAIN);
    }
    else
    {
        // [无数据] -> 提示配置
        lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0x222222), LV_PART_MAIN);
        lv_label_set_text(ui_LabelTrackStatus, "请用APP配置"); // SETUP VIA APP
        lv_label_set_text(ui_LabelTrackInfo, "无数据");        // NO DATA
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
    // 1. 单例检查：如果页面已经存在，只确保定时器开启，然后返回
    if (ui_ScreenTrackWait && lv_obj_is_valid(ui_ScreenTrackWait))
    {
        if (!timer_track_wait)
            timer_track_wait = lv_timer_create(track_wait_timer_cb, 100, NULL);
        return;
    }

    // 2. 确保字体样式已初始化
    ensure_styles_init();

    // 3. 创建屏幕容器
    ui_ScreenTrackWait = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenTrackWait, lv_color_hex(0x000000), LV_PART_MAIN);

    // ------------------------------------------------------------------------
    // UI 组件构建
    // ------------------------------------------------------------------------

    // [标题] 使用 36号 大字
    ui_LabelWaitTitle = lv_label_create(ui_ScreenTrackWait);
    lv_obj_add_style(ui_LabelWaitTitle, &style_text_30, 0);
    lv_label_set_text(ui_LabelWaitTitle, "等待起跑");
    // 局部覆盖颜色：使用醒目的橙色
    lv_obj_set_style_text_color(ui_LabelWaitTitle, lv_color_hex(0xFFA500), LV_PART_MAIN);
    lv_obj_align(ui_LabelWaitTitle, LV_ALIGN_TOP_MID, 0, 15);

    // [起点坐标] 使用 20号 小字
    // 逻辑：从 trackMgr 获取起点坐标并格式化显示
    ui_LabelWaitCoords = lv_label_create(ui_ScreenTrackWait);

    double sLat = 0.0, sLon = 0.0;
    // 假设 trackMgr 是全局对象
    trackMgr.getStartPoint(sLat, sLon);

    char coord_buf[64];
    // 简单的有效性检查 (绝对值 > 0.1 视为有效坐标)
    if (fabs(sLat) > 0.1)
    {
        snprintf(coord_buf, sizeof(coord_buf), "起点: %.5f, %.5f", sLat, sLon);
    }
    else
    {
        snprintf(coord_buf, sizeof(coord_buf), "起点: --.----, --.----");
    }

    lv_obj_add_style(ui_LabelWaitCoords, &style_text_14, 0);
    lv_label_set_text(ui_LabelWaitCoords, coord_buf);
    // 局部覆盖颜色：使用灰色
    lv_obj_set_style_text_color(ui_LabelWaitCoords, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    // Y位置下移到 65，避开上方的大标题
    lv_obj_align(ui_LabelWaitCoords, LV_ALIGN_TOP_MID, 0, 65);

    // [距离数值] 使用 36号 大字 (核心数据)
    ui_LabelWaitDist = lv_label_create(ui_ScreenTrackWait);
    lv_obj_add_style(ui_LabelWaitDist, &style_text_30, 0);
    // 初始显示占位符，由 timer 回调函数更新实际数值
    lv_label_set_text(ui_LabelWaitDist, "距离: --.- m");
    // 放在屏幕中心稍微偏上一点
    lv_obj_align(ui_LabelWaitDist, LV_ALIGN_CENTER, 0, -5);

    // [操作提示] 使用 20号 小字
    lv_obj_t *hint = lv_label_create(ui_ScreenTrackWait);
    lv_obj_add_style(hint, &style_text_14, 0);
    lv_label_set_text(hint, "请穿过起跑线\n自动开始计时");
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), LV_PART_MAIN); // 深灰色
    // 放在屏幕中心偏下
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 30);

    // [取消按钮]
    ui_BtnWaitExit = lv_btn_create(ui_ScreenTrackWait);
    // 尺寸加大以适应手指操作 (140x50)
    lv_obj_set_size(ui_BtnWaitExit, 140, 50);
    lv_obj_align(ui_BtnWaitExit, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(ui_BtnWaitExit, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_radius(ui_BtnWaitExit, 25, LV_PART_MAIN); // 大圆角
    lv_obj_add_event_cb(ui_BtnWaitExit, btn_wait_exit_event_cb, LV_EVENT_CLICKED, NULL);

    // [按钮文字] 使用 36号 大字
    lv_obj_t *lbl_exit = lv_label_create(ui_BtnWaitExit);
    lv_obj_add_style(lbl_exit, &style_text_30, 0);
    lv_label_set_text(lbl_exit, "取消");
    lv_obj_center(lbl_exit);

    // 4. 启动刷新定时器
    timer_track_wait = lv_timer_create(track_wait_timer_cb, 100, NULL);
}
void build_mode_page()
{
    if (ui_ScreenMode && lv_obj_is_valid(ui_ScreenMode))
        return;

    ensure_styles_init();

    ui_ScreenMode = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenMode, lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_add_event_cb(ui_ScreenMode, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    // [标题] Y=5 (贴近顶部)
    lv_obj_t *title = lv_label_create(ui_ScreenMode);
    lv_obj_add_style(title, &style_text_14, 0);
    lv_label_set_text(title, "选择模式");
    lv_obj_set_style_text_color(title, lv_color_hex(0xAAAAAA), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5); // <--- 修改点 1

    // [按钮 1] 街道漫游 Y=25 (紧贴标题)
    ui_BtnRoam = lv_btn_create(ui_ScreenMode);
    lv_obj_set_size(ui_BtnRoam, 280, 80);
    lv_obj_align(ui_BtnRoam, LV_ALIGN_TOP_MID, 0, 25); // <--- 修改点 2 (原40 -> 25)
    lv_obj_add_event_cb(ui_BtnRoam, btn_roam_event_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(ui_BtnRoam, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *lbl_roam = lv_label_create(ui_BtnRoam);
    lv_obj_add_style(lbl_roam, &style_text_30, 0);
    lv_label_set_text(lbl_roam, "街道漫游");
    lv_obj_align(lbl_roam, LV_ALIGN_TOP_LEFT, 10, 10);

    ui_LabelRoamStatus = lv_label_create(ui_BtnRoam);
    lv_obj_add_style(ui_LabelRoamStatus, &style_text_14, 0);
    lv_label_set_text(ui_LabelRoamStatus, "正在定位...");
    lv_obj_align(ui_LabelRoamStatus, LV_ALIGN_BOTTOM_RIGHT, -5, -5);

    // [按钮 2] 赛道模式 Y=115 (25起始 + 80高度 + 10间距 = 115)
    ui_BtnTrack = lv_btn_create(ui_ScreenMode);
    lv_obj_set_size(ui_BtnTrack, 280, 80);
    lv_obj_align(ui_BtnTrack, LV_ALIGN_TOP_MID, 0, 115); // <--- 修改点 3 (原140 -> 115)
    lv_obj_set_style_bg_color(ui_BtnTrack, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_add_event_cb(ui_BtnTrack, btn_track_event_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(ui_BtnTrack, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_t *lbl_track = lv_label_create(ui_BtnTrack);
    lv_obj_add_style(lbl_track, &style_text_30, 0);
    lv_label_set_text(lbl_track, "赛道模式");
    lv_obj_align(lbl_track, LV_ALIGN_TOP_LEFT, 10, 10);

    ui_LabelTrackStatus = lv_label_create(ui_BtnTrack);
    lv_obj_add_style(ui_LabelTrackStatus, &style_text_14, 0);
    lv_label_set_text(ui_LabelTrackStatus, "加载中...");
    lv_obj_align(ui_LabelTrackStatus, LV_ALIGN_BOTTOM_RIGHT, -5, -5);

    ui_LabelTrackInfo = lv_label_create(ui_BtnTrack);
    lv_label_set_text(ui_LabelTrackInfo, "-");
    lv_obj_add_style(ui_LabelTrackInfo, &style_text_14, 0);
    lv_obj_set_style_text_color(ui_LabelTrackInfo, lv_color_hex(0x888888), LV_PART_MAIN);
    lv_obj_align(ui_LabelTrackInfo, LV_ALIGN_BOTTOM_LEFT, 10, -5);

    lv_timer_create(mode_timer_cb, 500, NULL);
}