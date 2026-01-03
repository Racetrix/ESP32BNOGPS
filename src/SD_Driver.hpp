#pragma once

#include <Arduino.h>
#include "FS.h"
#include "SD_MMC.h"

// JC2432W328 的 SD 卡引脚
#define SD_CLK 38
#define SD_CMD 40
#define SD_D0 39
#define SD_D1 41
#define SD_D2 48
#define SD_D3 47

// --- 新增：全局状态标志 ---
extern bool sd_connected;

bool initSD()
{
    Serial.println("Mounting SD Card...");
    SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3);

    // 尝试挂载
    if (!SD_MMC.begin("/sdcard", false))
    {
        Serial.println("❌ SD Card Mount Failed!");
        sd_connected = false;
        return false;
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE)
    {
        Serial.println("❌ No SD Card attached");
        sd_connected = false;
        return false;
    }

    Serial.println("✅ SD Card Mounted!");
    sd_connected = true; // 标记为成功
    return true;
}