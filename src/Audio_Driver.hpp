#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Audio.h"
#include "SD_MMC.h"
#include "FS.h"
#include <vector> // 引入向量容器

// ==========================================
// ⚡️ 防卡顿配置：多线程 + 黄金参数
// ==========================================
#define I2S_DOUT 8
#define I2S_BCLK 5
#define I2S_LRCK 7
#define I2S_MCLK 4
#define I2S_MUTE_PIN 1

#define IIC_SDA 16
#define IIC_SCL 15
#define ES8311_ADDR 0x18

class Audio_Driver
{
private:
    SemaphoreHandle_t _mutex;      // 互斥锁，保护播放列表
    std::vector<String> _playlist; // 播放队列

    void writeReg(uint8_t reg, uint8_t data)
    {
        Wire.beginTransmission(ES8311_ADDR);
        Wire.write(reg);
        Wire.write(data);
        Wire.endTransmission();
    }

    void initES8311()
    {
        Serial.println("[ES8311] Init...");
        Wire.begin(IIC_SDA, IIC_SCL);
        Wire.setClock(400000);

        // 复位
        writeReg(0x00, 0x1F);
        delay(20);
        writeReg(0x00, 0x00);
        writeReg(0x00, 0x80);

        // 时钟配置 (44.1kHz)
        writeReg(0x01, 0x3F);
        writeReg(0x02, 0x00);
        writeReg(0x03, 0x10);
        writeReg(0x04, 0x10);
        writeReg(0x05, 0x00);
        writeReg(0x06, 0x03);
        writeReg(0x07, 0x00);
        writeReg(0x08, 0xFF);

        // 格式
        writeReg(0x09, 0x0C);
        writeReg(0x0A, 0x0C);

        // 开启模拟电路
        writeReg(0x0D, 0x01);
        writeReg(0x10, 0x1F);

        // 开启大推力 (HP Drive)
        writeReg(0x12, 0x00);
        writeReg(0x13, 0x10);

        // 增益设置
        writeReg(0x14, 0x10);
        writeReg(0x32, 0xBF);

        writeReg(0x37, 0x08);
    }

    // ---------------------------------------------------------
    // 🧵 独立音频任务 (运行在 Core 0)
    // ---------------------------------------------------------
    // ---------------------------------------------------------
    // 🧵 独立音频任务 (运行在 Core 0) - 极速版
    // ---------------------------------------------------------
    static void audioTask(void *parameter)
    {
        Audio_Driver *driver = (Audio_Driver *)parameter;

        while (true)
        {
            // 1. 核心循环：驱动音频库
            driver->audio.loop();

            // 2. 队列管理逻辑
            // 只有当音乐停止时，才去检查队列
            if (!driver->audio.isRunning())
            {
                // 尝试获取锁 (不等待，能拿就拿，拿不到下一圈再试，保证 loop 不卡顿)
                if (xSemaphoreTake(driver->_mutex, 0) == pdTRUE)
                {
                    if (!driver->_playlist.empty())
                    {
                        // 取出第一首
                        String nextFile = driver->_playlist.front();
                        driver->_playlist.erase(driver->_playlist.begin());

                        // ❌ 删除这里的 Serial.print，它会严重阻塞 CPU！

                        if (SD_MMC.exists(nextFile))
                        {
                            // ⚡️ 核心优化：直接连接，不打印日志
                            driver->audio.connecttoFS(SD_MMC, nextFile.c_str());
                            driver->isPlaying = true;
                        }
                    }
                    else
                    {
                        driver->isPlaying = false;
                    }

                    xSemaphoreGive(driver->_mutex);
                }

                // 空闲状态：没有在播放，也没歌了，可以休息久一点省电
                if (!driver->isPlaying)
                {
                    vTaskDelay(10);
                }
            }
            else
            {
                // ⚡️ 播放状态：全速运行！
                // 移除 vTaskDelay(1)，给音频库最大的 CPU 时间片
                // 只有在 ESP32 只有单核时才需要 delay，双核 Core 0 独占时不需要
                // 这里的 yield 是为了防止看门狗复位，但比 delay 快得多
                taskYIELD();
            }
        }
    }

public:
    Audio audio;
    bool isPlaying = false; // 指示是否有任务正在进行（包括队列中）

    Audio_Driver()
    {
        // 创建互斥锁
        _mutex = xSemaphoreCreateMutex();
    }

    void begin()
    {
        initES8311();

        // 开启功放
        pinMode(I2S_MUTE_PIN, OUTPUT);
        digitalWrite(I2S_MUTE_PIN, LOW);

        // I2S 配置
        audio.setPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT, -1, I2S_MCLK);

        // 设置默认音量
        setVolume(10);

        // ---------------------------------------------------------
        // 🚀 创建独立任务
        // ---------------------------------------------------------
        xTaskCreatePinnedToCore(
            audioTask,
            "AudioTask",
            4096,
            this,
            20,
            NULL,
            0);

        Serial.println("[Audio] Running on Core 0 (Queue Enabled)");
    }

    // 修改后的 play：只负责加入队列
    void play(String filename)
    {
        // 获取互斥锁 (无限等待直到获取到锁)
        xSemaphoreTake(_mutex, portMAX_DELAY);

        // 加入队列
        _playlist.push_back(filename);

        // 标记为正在播放状态 (实际上可能还没开始，但在排队了)
        isPlaying = true;

        // 释放锁
        xSemaphoreGive(_mutex);
    }

    // 兼容旧的 const char* 调用
    void play(const char *filename)
    {
        play(String(filename));
    }

    // 紧急停止 (清空队列并停止当前播放)
    void stop()
    {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        _playlist.clear(); // 清空队列
        xSemaphoreGive(_mutex);

        audio.stopSong(); // 停止当前
        isPlaying = false;
    }

    // 主循环接口 (保留但留空)
    void loop()
    {
        // 空函数，逻辑都在 Task 里
    }

    void setVolume(uint8_t vol)
    {
        if (vol > 21)
            vol = 21;
        audio.setVolume(vol);
        uint8_t chip_vol = ::map(vol, 0, 21, 0, 255);
        writeReg(0x32, chip_vol);
    }
};

Audio_Driver audioDriver;