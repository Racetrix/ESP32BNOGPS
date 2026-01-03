#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// 启用 LVGL
#define LV_CONF_SKIP 0

// 屏幕尺寸
#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 320

// 颜色深度 (16bit RGB565)
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0 // 如果颜色红蓝反了，改这里为 1

// 内存管理
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (128U * 1024U) // 给 LVGL 内部分配 128KB 堆内存
#define LV_MEM_ADR 0
#define LV_MEM_BUF_MAX_NUM 16

// 刷新率限制 (避免过快刷新浪费资源)
// #define LV_DISP_DEF_REFR_PERIOD 16 // ~60fps
#define LV_DISP_DEF_REFR_PERIOD 28

// 启用日志 (方便调试)
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_INFO

// 字体
/*==================
 * FONT USAGE
 *================*/

/* Montserrat fonts with ASCII range and some symbols using bpp = 4
 * https://fonts.google.com/specimen/Montserrat */
#define LV_FONT_MONTSERRAT_8 0
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 1 // <--- 确保这个是 1 (小字)
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 1 // <--- 把这个改成 1 (中号字，我刚才用的就是这个)
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 1 // <--- 强烈建议把这个也改成 1 (特大号字)

// 启用 FPS 监视器 (会在屏幕角落显示帧率)
// #define LV_USE_PERF_MONITOR 1
// #define LV_USE_MEM_MONITOR 0

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#endif