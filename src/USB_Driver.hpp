#pragma once
#include <Arduino.h>
#include "USB.h"
#include "USBMSC.h"
#include "SD_MMC.h"
#include "LGFX_Driver.hpp"

// 引入底层驱动
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "System_Config.hpp"
// 引用外部对象
extern LGFX tft;
extern ConfigManager sys_cfg;

// USB MSC 对象
USBMSC msc;

// ==========================================
// [黑魔法区域] 暴力破解私有成员访问权限
// ==========================================
namespace fs
{
    // 定义一个替身类，继承自 FS (和原版 SDMMCFS 一样)
    // 利用 C++ 内存布局特性，强行映射 _card 变量
    class SDMMCFS_HACK : public FS
    {
    public:
        sdmmc_card_t *_card; // 这里把它定义为 public
    };
}

// 辅助函数：获取私有的 _card 指针
sdmmc_card_t *get_sd_card_handle()
{
    // 将 SD_MMC 强转为我们的替身类，然后访问 _card
    return ((fs::SDMMCFS_HACK *)&SD_MMC)->_card;
}
// ==========================================

// 读回调
static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
    uint32_t count = bufsize / 512;
    // 使用破解得到的句柄
    esp_err_t err = sdmmc_read_sectors(get_sd_card_handle(), buffer, lba, count);
    if (err == ESP_OK)
        return bufsize;
    return -1;
}

// 写回调
static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
    uint32_t count = bufsize / 512;
    // 使用破解得到的句柄
    esp_err_t err = sdmmc_write_sectors(get_sd_card_handle(), buffer, lba, count);
    if (err == ESP_OK)
        return bufsize;
    return -1;
}

// USB 模式的主循环
void run_usb_mode()
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(20, 100);
    tft.println("USB DISK MODE");
    tft.setCursor(20, 140);
    tft.println("Connect PC to USB Port");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(20, 200);
    tft.println("Tap Screen to EXIT");

    Serial.println("Starting USB MSC...");

    // 1. 设置 ID
    msc.vendorID("RACE");
    msc.productID("TAXI_BOX");
    msc.productRevision("1.0");

    // 2. 绑定回调
    msc.onRead(onRead);
    msc.onWrite(onWrite);

    // 3. 启用
    msc.mediaPresent(true);

    // 计算扇区数
    uint32_t sectorCount = SD_MMC.cardSize() / 512;
    msc.begin(sectorCount, 512);

    USB.begin();

    // 循环等待退出
    while (true)
    {
        uint16_t x, y;
        if (tft.getTouch(&x, &y))
        {
            tft.fillScreen(TFT_BLACK);
            tft.setCursor(20, 120);
            tft.setTextColor(TFT_RED);
            tft.println("Rebooting...");

            sys_cfg.boot_into_usb = false;
            sys_cfg.save();

            delay(500);
            ESP.restart();
        }
        delay(10);
    }
}