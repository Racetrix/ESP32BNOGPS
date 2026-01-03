#pragma once

#include <lvgl.h>
#include <vector>
#include "LGFX_Driver.hpp"
#include "SD_Driver.hpp"
#include "GPS_Driver.hpp"
#include "IMU_Driver.hpp"
#include "System_Config.hpp"
#include "Settings_UI.hpp"
#include "ModeSelect_UI.hpp"
#include "BLE_Driver.hpp"
#include "DragRace_UI.hpp"

// 引用外部对象
extern LGFX tft;
extern IMU_Driver imu;
extern bool sd_connected;
extern BLE_Driver ble;

// --- LVGL 缓冲 ---
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[240 * 40];
static lv_color_t buf2[240 * 40];
static bool is_boot_anim_active = false; // [新增] 动画状态标志

// --- 全局 UI 对象 (全部声明为全局变量) ---
lv_obj_t *ui_ScreenMain = NULL;
lv_obj_t *ui_ScreenBoot = NULL;
// ui_ScreenSettings 和 ui_ScreenMode 在各自头文件中定义，这里不需要

lv_obj_t *ui_LabelSpeed = NULL;
lv_obj_t *ui_ArcSpeed = NULL;
lv_obj_t *ui_ObjGball = NULL;
lv_obj_t *ui_LabelTimeVal = NULL;
lv_obj_t *ui_LabelSatVal = NULL;

// [关键统一] 使用 Label 指针
lv_obj_t *ui_LabelSD = NULL;
lv_obj_t *ui_LabelBLE = NULL;
lv_obj_t *ui_LabelIMU = NULL;

// --- 样式 ---
lv_style_t style_label_gray;
lv_style_t style_label_cyan;
lv_style_t style_box_outline;

lv_style_t style_tech_text;
lv_style_t style_tech_bar_bg;
lv_style_t style_tech_bar_indic;

// 前向声明
void build_dashboard(void);
void build_boot_screen(void);
void start_dashboard_sweep(void);
void build_drag_page(void);

// 屏幕刷新回调
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.writePixels((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();
    lv_disp_flush_ready(disp);
}

// 触摸读取回调
void my_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchX, &touchY);
    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

// 手势事件
void screen_gesture_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *current_scr = lv_scr_act(); // 获取当前屏幕

    if (code == LV_EVENT_GESTURE)
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

        // 1. 如果当前在 [主页]
        if (current_scr == ui_ScreenMain)
        {
            if (dir == LV_DIR_LEFT) // 左滑 -> 去模式选择 (第2屏)
            {
                if (!ui_ScreenMode)
                    build_mode_page();
                lv_scr_load_anim(ui_ScreenMode, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
            }
            // 右滑 -> 去设置 (如果有的话)
            else if (dir == LV_DIR_RIGHT)
            {
                if (!ui_ScreenSettings)
                    build_settings_page();
                lv_scr_load_anim(ui_ScreenSettings, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
            }
        }

        // 2. 如果当前在 [模式选择页] (第2屏)
        else if (current_scr == ui_ScreenMode)
        {
            if (dir == LV_DIR_RIGHT) // 右滑 -> 返回主页
            {
                lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
            }
            else if (dir == LV_DIR_LEFT) // [新增] 左滑 -> 去零百测试 (第3屏)
            {
                if (!ui_ScreenDrag)
                    build_drag_page();
                // 动画 MOVE_LEFT 表示新页面从右边进来
                lv_scr_load_anim(ui_ScreenDrag, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
            }
        }

        // 3. 如果当前在 [零百测试页] (第3屏)
        else if (current_scr == ui_ScreenDrag)
        {
            if (dir == LV_DIR_RIGHT) // [新增] 右滑 -> 返回模式选择
            {
                // 停止零百页面的定时器以省电 (可选)
                if (timer_drag_refresh)
                {
                    lv_timer_del(timer_drag_refresh);
                    timer_drag_refresh = NULL;
                }

                // 确保 Mode 页面还在
                if (!ui_ScreenMode)
                    build_mode_page();

                // 动画 MOVE_RIGHT 表示新页面从左边回来
                lv_scr_load_anim(ui_ScreenMode, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
            }
        }

        // 4. 设置页 (略)
        else if (current_scr == ui_ScreenSettings && dir == LV_DIR_LEFT)
        {
            lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        }
    }
}
// ================= 开机动画逻辑 =================

// 动画定时器回调
// 动画定时器回调
void boot_timer_cb(lv_timer_t *timer)
{
    lv_obj_t *bar = (lv_obj_t *)timer->user_data;
    int32_t val = lv_bar_get_value(bar);

    // 非线性加载速度：开始快，中间慢，最后快，模拟真实系统加载
    if (val < 30)
        val += 3;
    else if (val < 80)
        val += 1; // 中间卡顿一下，更有感觉
    else
        val += 4;

    if (val <= 100)
    {
        lv_bar_set_value(bar, val, LV_ANIM_ON);

        // 可选：让文字闪烁或变化，这里简单处理
    }
    else
    {
        // 进度跑完
        lv_timer_del(timer);
        build_dashboard();
        // 使用淡入效果切换
        lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_NONE, 800, 0, true);
        start_dashboard_sweep();
    }
}

// 构建启动屏
void build_boot_screen()
{
    ui_ScreenBoot = lv_obj_create(NULL);
    // 纯黑背景
    lv_obj_set_style_bg_color(ui_ScreenBoot, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_clear_flag(ui_ScreenBoot, LV_OBJ_FLAG_SCROLLABLE);

    // === 1. 主 Logo: RACETRIX ===
    lv_obj_t *lbl_logo = lv_label_create(ui_ScreenBoot);
    lv_label_set_text(lbl_logo, "RACETRIX");
    // 使用最大字体
    lv_obj_set_style_text_font(lbl_logo, &lv_font_montserrat_48, LV_PART_MAIN);
    // 应用我们定义的科技样式
    lv_obj_add_style(lbl_logo, &style_tech_text, LV_PART_MAIN);
    // 居中稍微偏上
    lv_obj_align(lbl_logo, LV_ALIGN_CENTER, 0, -30);

    // === 2. 装饰性文字 (模拟系统自检) ===
    lv_obj_t *lbl_sys = lv_label_create(ui_ScreenBoot);
    lv_label_set_text(lbl_sys, "SYSTEM INITIALIZING...");
    lv_obj_set_style_text_font(lbl_sys, &lv_font_montserrat_14, LV_PART_MAIN);
    // 稍微暗一点的绿色
    lv_obj_set_style_text_color(lbl_sys, lv_color_hex(0x008f30), LV_PART_MAIN);
    // 增加字间距
    lv_obj_set_style_text_letter_space(lbl_sys, 2, LV_PART_MAIN);
    lv_obj_align(lbl_sys, LV_ALIGN_CENTER, 0, 10);

    // === 3. 版本号 (右下角) ===
    lv_obj_t *lbl_ver = lv_label_create(ui_ScreenBoot);
    lv_label_set_text(lbl_ver, "v1.0.2 RC");
    lv_obj_set_style_text_font(lbl_ver, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_ver, lv_color_hex(0x333333), LV_PART_MAIN); // 深灰
    lv_obj_align(lbl_ver, LV_ALIGN_BOTTOM_RIGHT, -10, -5);

    // === 4. 科技感进度条 ===
    lv_obj_t *bar = lv_bar_create(ui_ScreenBoot);
    // 细长条
    lv_obj_set_size(bar, 240, 4);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 60); // 放在文字下方
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);

    // 应用样式
    lv_obj_add_style(bar, &style_tech_bar_bg, LV_PART_MAIN);
    lv_obj_add_style(bar, &style_tech_bar_indic, LV_PART_INDICATOR);

    // 创建定时器 (速度稍快一点，30ms)
    lv_timer_create(boot_timer_cb, 30, bar);
}
void init_styles()
{
    lv_style_init(&style_label_gray);
    lv_style_set_text_color(&style_label_gray, lv_color_hex(0x888888));
    lv_style_set_text_font(&style_label_gray, &lv_font_montserrat_14);

    lv_style_init(&style_label_cyan);
    lv_style_set_text_color(&style_label_cyan, lv_color_hex(0x00AEEF));
    lv_style_set_text_font(&style_label_cyan, &lv_font_montserrat_20);

    lv_style_init(&style_box_outline);
    lv_style_set_bg_opa(&style_box_outline, 0);
    lv_style_set_border_width(&style_box_outline, 1);
    lv_style_set_border_color(&style_box_outline, lv_color_hex(0x444444));
    lv_style_set_radius(&style_box_outline, 4);
    lv_style_set_text_color(&style_box_outline, lv_color_hex(0x555555));
    lv_style_set_text_font(&style_box_outline, &lv_font_montserrat_14);

    // 1. 科技文字样式 (暗绿色 + 宽间距)
    lv_style_init(&style_tech_text);
    // 使用一种更有质感的“终端绿” (0x00EF50)
    lv_style_set_text_color(&style_tech_text, lv_color_hex(0x00EF50));
    // 增加字间距，营造电影里的字幕感
    lv_style_set_text_letter_space(&style_tech_text, 4);

    // 2. 进度条背景 (深灰细线)
    lv_style_init(&style_tech_bar_bg);
    lv_style_set_bg_color(&style_tech_bar_bg, lv_color_hex(0x1a1a1a));
    lv_style_set_radius(&style_tech_bar_bg, 0); // 直角更有科技感
    lv_style_set_bg_opa(&style_tech_bar_bg, 255);

    // 3. 进度条指示器 (亮绿)
    lv_style_init(&style_tech_bar_indic);
    lv_style_set_bg_color(&style_tech_bar_indic, lv_color_hex(0x00EF50));
    lv_style_set_radius(&style_tech_bar_indic, 0);
    // 添加一点阴影模拟发光效果 (注意：会稍微增加渲染负担，但在启动屏没关系)
    lv_style_set_shadow_color(&style_tech_bar_indic, lv_color_hex(0x00EF50));
    lv_style_set_shadow_width(&style_tech_bar_indic, 10);
    lv_style_set_shadow_spread(&style_tech_bar_indic, 1);
}

void create_divider(lv_obj_t *parent, int x_offset)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, 1, 16);
    lv_obj_set_style_bg_color(line, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
    lv_obj_align(line, LV_ALIGN_LEFT_MID, x_offset, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

// 构建主仪表盘
// 构建主仪表盘
void build_dashboard()
{
    ui_ScreenMain = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenMain, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_clear_flag(ui_ScreenMain, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui_ScreenMain, screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);

    // === 1. 左侧速度区域 ===
    lv_obj_t *panel_left = lv_obj_create(ui_ScreenMain);
    lv_obj_set_size(panel_left, 180, 240);
    lv_obj_align(panel_left, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(panel_left, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel_left, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel_left, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_border_side(panel_left, LV_BORDER_SIDE_RIGHT, LV_PART_MAIN);
    lv_obj_set_style_radius(panel_left, 0, LV_PART_MAIN);
    lv_obj_clear_flag(panel_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(panel_left, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_all(panel_left, 0, LV_PART_MAIN); // 清除内边距

    ui_ArcSpeed = lv_arc_create(panel_left);
    lv_obj_set_size(ui_ArcSpeed, 160, 160);
    lv_arc_set_rotation(ui_ArcSpeed, 135);
    lv_arc_set_bg_angles(ui_ArcSpeed, 0, 270);
    lv_arc_set_value(ui_ArcSpeed, 0);
    lv_obj_center(ui_ArcSpeed);
    lv_obj_set_style_arc_width(ui_ArcSpeed, 16, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_ArcSpeed, 16, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui_ArcSpeed, lv_color_hex(0x222222), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui_ArcSpeed, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
    lv_obj_remove_style(ui_ArcSpeed, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(ui_ArcSpeed, LV_OBJ_FLAG_CLICKABLE);

    ui_LabelSpeed = lv_label_create(panel_left);
    lv_label_set_text(ui_LabelSpeed, "0");
    lv_obj_set_style_text_font(ui_LabelSpeed, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelSpeed, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_align(ui_LabelSpeed, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *label_unit = lv_label_create(panel_left);
    lv_label_set_text(label_unit, "KM/H");
    lv_obj_add_style(label_unit, &style_label_cyan, LV_PART_MAIN);
    lv_obj_set_style_text_font(label_unit, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(label_unit, LV_ALIGN_CENTER, 0, 35);

    // === 2. 右上 G-Force ===
    lv_obj_t *panel_g = lv_obj_create(ui_ScreenMain);
    lv_obj_set_size(panel_g, 140, 140);
    lv_obj_align(panel_g, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(panel_g, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel_g, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel_g, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_border_side(panel_g, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel_g, 0, LV_PART_MAIN);
    lv_obj_clear_flag(panel_g, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *g_circle = lv_obj_create(panel_g);
    lv_obj_set_size(g_circle, 90, 90);
    lv_obj_align(g_circle, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_opa(g_circle, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_circle, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_circle, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_radius(g_circle, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    // 确保圆圈本身不能滚动
    lv_obj_clear_flag(g_circle, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(g_circle, LV_SCROLLBAR_MODE_OFF);

    // --- [修复 2] 竖线去除滚动条 ---
    lv_obj_t *line_v = lv_obj_create(g_circle);
    lv_obj_set_size(line_v, 1, 90);
    lv_obj_center(line_v);
    lv_obj_set_style_bg_color(line_v, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_border_width(line_v, 0, LV_PART_MAIN);
    lv_obj_clear_flag(line_v, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE); // 禁止滚动/点击
    lv_obj_set_scrollbar_mode(line_v, LV_SCROLLBAR_MODE_OFF);                  // 隐藏滚动条

    // --- [修复 2] 横线去除滚动条 ---
    lv_obj_t *line_h = lv_obj_create(g_circle);
    lv_obj_set_size(line_h, 90, 1);
    lv_obj_center(line_h);
    lv_obj_set_style_bg_color(line_h, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_border_width(line_h, 0, LV_PART_MAIN);
    lv_obj_clear_flag(line_h, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE); // 禁止滚动/点击
    lv_obj_set_scrollbar_mode(line_h, LV_SCROLLBAR_MODE_OFF);                  // 隐藏滚动条

    lv_obj_t *label_g = lv_label_create(panel_g);
    lv_label_set_text(label_g, "G-FORCE");
    lv_obj_add_style(label_g, &style_label_gray, LV_PART_MAIN);
    lv_obj_align(label_g, LV_ALIGN_BOTTOM_RIGHT, -10, -5);

    ui_ObjGball = lv_obj_create(panel_g);
    lv_obj_set_size(ui_ObjGball, 10, 10);
    lv_obj_set_style_radius(ui_ObjGball, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_ObjGball, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_align(ui_ObjGball, LV_ALIGN_CENTER, 0, -10);
    // G球也不能滚动
    lv_obj_clear_flag(ui_ObjGball, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // === 3. 右下数据区 ===
    lv_obj_t *panel_data = lv_obj_create(ui_ScreenMain);
    lv_obj_set_size(panel_data, 140, 100);
    lv_obj_align(panel_data, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(panel_data, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel_data, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel_data, 0, LV_PART_MAIN);
    lv_obj_clear_flag(panel_data, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_time = lv_label_create(panel_data);
    lv_label_set_text(lbl_time, "TIME");
    lv_obj_add_style(lbl_time, &style_label_gray, LV_PART_MAIN);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_LEFT, 10, 5);

    ui_LabelTimeVal = lv_label_create(panel_data);
    lv_label_set_text(ui_LabelTimeVal, "00:00");
    lv_obj_add_style(ui_LabelTimeVal, &style_label_cyan, LV_PART_MAIN);
    lv_obj_align(ui_LabelTimeVal, LV_ALIGN_TOP_RIGHT, -10, 3);

    lv_obj_t *lbl_gps = lv_label_create(panel_data);
    lv_label_set_text(lbl_gps, "GPS");
    lv_obj_add_style(lbl_gps, &style_label_gray, LV_PART_MAIN);
    lv_obj_align(lbl_gps, LV_ALIGN_TOP_LEFT, 10, 42);

    ui_LabelSatVal = lv_label_create(panel_data);
    lv_label_set_text(ui_LabelSatVal, "0 SAT");
    lv_obj_set_style_text_color(ui_LabelSatVal, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_LabelSatVal, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(ui_LabelSatVal, LV_ALIGN_TOP_RIGHT, -10, 42);

    // 状态栏
    lv_obj_t *status_bar = lv_obj_create(panel_data);
    lv_obj_set_size(status_bar, 122, 26);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_style(status_bar, &style_box_outline, LV_PART_MAIN);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(status_bar, LV_SCROLLBAR_MODE_OFF);

    // SD (X=14)
    ui_LabelSD = lv_label_create(status_bar);
    lv_label_set_text(ui_LabelSD, "SD");
    lv_obj_set_style_text_font(ui_LabelSD, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelSD, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_align(ui_LabelSD, LV_ALIGN_LEFT_MID, -5, 0);

    // 分割线 1 (位置 45)
    create_divider(status_bar, 22);

    // 2. BAT (正中间)
    ui_LabelBLE = lv_label_create(status_bar);
    lv_label_set_text(ui_LabelBLE, "BLE");
    lv_obj_set_style_text_font(ui_LabelBLE, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelBLE, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_align(ui_LabelBLE, LV_ALIGN_CENTER, -3, 0);

    // 分割线 2 (位置 91)
    create_divider(status_bar, 65);

    // 3. IMU (右侧区域)
    // 避免短线跑到字上面，往右挪一点
    ui_LabelIMU = lv_label_create(status_bar);
    lv_label_set_text(ui_LabelIMU, "IMU");
    lv_obj_set_style_text_font(ui_LabelIMU, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_LabelIMU, lv_color_hex(0x555555), LV_PART_MAIN);
    // 稍微靠右对齐，Offset -12px
    lv_obj_align(ui_LabelIMU, LV_ALIGN_RIGHT_MID, 5, 0);
}

void init_ui()
{
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, buf2, 240 * 40);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touch_read;
    lv_indev_drv_register(&indev_drv);

    init_styles();

    // [修改] 先加载开机动画
    build_boot_screen();

    // 加载 Boot 屏
    lv_scr_load(ui_ScreenBoot);
}

// 状态更新函数 (带空指针检查)
void update_dev_status()
{
    // SD 卡状态
    if (ui_LabelSD)
    {
        lv_color_t color = sd_connected ? lv_color_hex(0x00FF00) : lv_color_hex(0x555555);
        lv_obj_set_style_text_color(ui_LabelSD, color, LV_PART_MAIN);
    }

    // [修改] BLE 状态
    if (ui_LabelBLE)
    {
        // 如果连接：显示青色(蓝牙色)；未连接：显示灰色
        lv_color_t color = ble.isConnected() ? lv_color_hex(0x00AEEF) : lv_color_hex(0x555555);
        lv_obj_set_style_text_color(ui_LabelBLE, color, LV_PART_MAIN);
    }

    // IMU 状态
    if (ui_LabelIMU)
    {
        lv_color_t color = imu.isConnected ? lv_color_hex(0x00FF00) : lv_color_hex(0x555555);
        lv_obj_set_style_text_color(ui_LabelIMU, color, LV_PART_MAIN);
    }
}
// UI 循环更新
void update_ui_loop()
{
    lv_timer_handler();

    // static uint32_t last_ball_update = 0;
    // if (millis() - last_ball_update > 30)
    // {
    //     last_ball_update = millis();

    //     if (ui_ObjGball)
    //     {
    //         float gx = imu.ax;
    //         float gy = imu.ay;
    //         // 应用配置交换
    //         if (sys_cfg.imu_swap_axis)
    //         {
    //             float t = gx;
    //             gx = gy;
    //             gy = t;
    //         }
    //         if (sys_cfg.imu_invert_x)
    //             gx = -gx;
    //         if (sys_cfg.imu_invert_y)
    //             gy = -gy;

    //         if (gx > 1.5)
    //             gx = 1.5;
    //         if (gx < -1.5)
    //             gx = -1.5;
    //         if (gy > 1.5)
    //             gy = 1.5;
    //         if (gy < -1.5)
    //             gy = -1.5;
    //         int offset_x = (int)(gx * 30);
    //         int offset_y = (int)(gy * 30);
    //         lv_obj_align(ui_ObjGball, LV_ALIGN_CENTER, offset_x, offset_y - 10);
    //     }
    // }
    // [修改 2] 性能优化：只有在主界面时才计算 G球
    // 如果当前屏幕是设置页，就别浪费 CPU 算 G球了，全力负责渲染列表滑动
    if (lv_scr_act() != ui_ScreenMain)
        return;

    static uint32_t last_ball_update = 0;
    if (millis() - last_ball_update > 30)
    {
        last_ball_update = millis();

        if (ui_ObjGball)
        {
            float gx = imu.lat_g; // 横向 G (对应原来的 ax)
            float gy = imu.lon_g; // 纵向 G (对应原来的 ay)
            // 应用配置交换
            // if (sys_cfg.imu_swap_axis)
            // {
            //     float t = gx;
            //     gx = gy;
            //     gy = t;
            // }
            // if (sys_cfg.imu_invert_x)
            //     gx = -gx;
            // if (sys_cfg.imu_invert_y)
            //     gy = -gy;

            if (gx > 1.5)
                gx = 1.5;
            if (gx < -1.5)
                gx = -1.5;
            if (gy > 1.5)
                gy = 1.5;
            if (gy < -1.5)
                gy = -1.5;
            int offset_x = (int)(gx * 30);
            int offset_y = (int)(gy * 30);
            lv_obj_align(ui_ObjGball, LV_ALIGN_CENTER, offset_x, offset_y - 10);
        }
    }
}

// 刷新数值 (主循环调用)
// 刷新数值 (主循环调用)
void ui_refresh_data()
{
    // [动画保护] 如果正在播放开机扫表动画，禁止刷新数据
    if (is_boot_anim_active)
        return;

    // --- [修改] 速度与圆弧联动逻辑 ---
    if (ui_LabelSpeed)
    {
        float display_speed = 0.0;

        // 1. 获取有效速度
        if (gps.tgps.location.isValid() && gps.getSatellites() >= 5)
        {
            float raw_speed = gps.getSpeed();
            if (raw_speed < 3.0)
                display_speed = 0.0;
            else
                display_speed = raw_speed;
        }
        else
        {
            display_speed = 0.0;
        }

        // 2. 更新数字显示 (使用 snprintf 修复 f 问题)
        char spd_buf[16];
        snprintf(spd_buf, sizeof(spd_buf), "%.1f", display_speed);
        lv_label_set_text(ui_LabelSpeed, spd_buf);

        // 3. [新增] 更新圆弧动画 (联动)
        if (ui_ArcSpeed)
        {
            const float MAX_SPEED_DISPLAY = 240.0; // 表底速度，与开机动画保持一致

            // 计算百分比 (0-100)
            int arc_val = (int)((display_speed / MAX_SPEED_DISPLAY) * 100);

            // 限制范围，防止爆表
            if (arc_val > 100)
                arc_val = 100;
            if (arc_val < 0)
                arc_val = 0;

            // 设置圆弧值 (LVGL 会自动处理平滑过渡，如果开启了动画的话)
            lv_arc_set_value(ui_ArcSpeed, arc_val);

            // [可选] 如果你想让圆弧颜色也随速度变化（比如超过200变红）
            if (display_speed > 200)
            {
                lv_obj_set_style_arc_color(ui_ArcSpeed, lv_color_hex(0xFF0000), LV_PART_INDICATOR);
            }
            else
            {
                lv_obj_set_style_arc_color(ui_ArcSpeed, lv_color_hex(0xFFFFFF), LV_PART_INDICATOR);
            }
        }

        // 4. [新增] 文字动态变色 (超过 200km/h 变红)
        if (display_speed > 200)
        {
            lv_obj_set_style_text_color(ui_LabelSpeed, lv_color_hex(0xFF0000), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_text_color(ui_LabelSpeed, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        }
    }

    // --- 卫星状态显示 ---
    if (ui_LabelSatVal)
    {
        int sats = gps.getSatellites();
        lv_label_set_text_fmt(ui_LabelSatVal, "%d SAT", sats);

        if (sats == 0)
            lv_obj_set_style_text_color(ui_LabelSatVal, lv_color_hex(0xFF0000), LV_PART_MAIN);
        else if (!gps.tgps.location.isValid() || sats < 5)
            lv_obj_set_style_text_color(ui_LabelSatVal, lv_color_hex(0xFFFF00), LV_PART_MAIN);
        else
            lv_obj_set_style_text_color(ui_LabelSatVal, lv_color_hex(0x00FF00), LV_PART_MAIN);
    }

    // --- 设备状态 ---
    update_dev_status();

    // --- 时间显示 ---
    if (ui_LabelTimeVal)
    {
        if (sys_cfg.is_running)
        {
            uint32_t elapsed = millis() - sys_cfg.session_start_ms;
            uint32_t total_seconds = elapsed / 1000;
            uint32_t mm = (total_seconds / 60) % 60;
            uint32_t ss = total_seconds % 60;
            lv_label_set_text_fmt(ui_LabelTimeVal, "%02d:%02d", mm, ss);
            lv_obj_set_style_text_color(ui_LabelTimeVal, lv_color_hex(0x00FF00), LV_PART_MAIN);
        }
        else
        {
            lv_label_set_text(ui_LabelTimeVal, "00:00");
            lv_obj_set_style_text_color(ui_LabelTimeVal, lv_color_hex(0x00AEEF), LV_PART_MAIN);
        }
    }
}

// 仪表盘自检动画回调
// 仪表盘自检动画回调
void dashboard_sweep_cb(lv_timer_t *timer)
{
    static int val = 0;        // [修改] 改为 int，解决 % 报错
    static int step = 2;       // 初始步进
    static int phase = 0;      // 0:上升, 1:停顿测试四位数, 2:下降
    static int hold_count = 0; // 停顿计时

    // 获取仪表盘最大刻度
    const int MAX_SPEED = 240;
    const int ARC_MAX = 100;

    if (phase == 0) // === 上升阶段 ===
    {
        // 模拟非线性加速：根据当前速度决定步进
        if (val < 40)
            step = 2;
        else if (val < 100)
            step = 5;
        else
            step = 8;

        val += step;

        if (val >= MAX_SPEED)
        {
            val = MAX_SPEED;
            phase = 1; // 进入停顿测试
        }
    }
    else if (phase == 1) // === 极限宽度测试阶段 (显示 8888) ===
    {
        hold_count++;
        // 在顶点强制显示 8888，测试布局宽度
        lv_label_set_text(ui_LabelSpeed, "8888");

        // 同时把 Arc 拉满
        lv_arc_set_value(ui_ArcSpeed, 100);

        // 让 8888 闪烁一下颜色，更显眼
        if (hold_count % 5 < 3)
        {
            lv_obj_set_style_text_color(ui_LabelSpeed, lv_color_hex(0xFF0000), LV_PART_MAIN);
        }
        else
        {
            lv_obj_set_style_text_color(ui_LabelSpeed, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        }

        if (hold_count > 20) // 停留约 400ms
        {
            phase = 2;
            step = 8; // 下降初始速度
        }
        return; // 这一帧只显示 8888，不执行下面的普通显示逻辑
    }
    else if (phase == 2) // === 下降阶段 ===
    {
        // 模拟减速阻尼
        if (val > 100)
            step = 8;
        else if (val > 40)
            step = 5;
        else
            step = 2;

        val -= step;

        if (val <= 0)
        {
            val = 0;
            // 动画结束
            is_boot_anim_active = false; // 释放控制权

            // 重置状态，以便下次还能跑
            phase = 0;
            hold_count = 0;

            lv_timer_del(timer); // 删除定时器
        }
    }

    // --- 更新 UI ---

    // 1. 更新圆弧 (将 0-MAX 映射到 0-100%)
    int arc_val = (int)(((float)val / MAX_SPEED) * ARC_MAX);
    lv_arc_set_value(ui_ArcSpeed, arc_val);

    // 2. 更新数字 (带一位小数)
    // [注意] 这里要把 int val 强转为 float，否则 %.1f 格式化会出错
    char spd_buf[16];
    snprintf(spd_buf, sizeof(spd_buf), "%.1f", (float)val);
    lv_label_set_text(ui_LabelSpeed, spd_buf);

    // 变色逻辑
    if (val > 200)
        lv_obj_set_style_text_color(ui_LabelSpeed, lv_color_hex(0xFF0000), LV_PART_MAIN); // 红色
    else
        lv_obj_set_style_text_color(ui_LabelSpeed, lv_color_hex(0xFFFFFF), LV_PART_MAIN); // 白色
}
// 启动扫表动画 (在进入主界面时调用)
void start_dashboard_sweep()
{
    is_boot_anim_active = true; // 抢占 UI 控制权

    // 创建一个 20ms 刷新一次的定时器，保证动画丝滑
    lv_timer_create(dashboard_sweep_cb, 20, NULL);
}