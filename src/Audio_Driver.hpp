#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Audio.h"
#include "SD_MMC.h"
#include "FS.h"

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
    // 🧵 新增：独立音频任务 (运行在 Core 0)
    // ---------------------------------------------------------
    static void audioTask(void *parameter)
    {
        Audio_Driver *driver = (Audio_Driver *)parameter;
        while (true)
        {
            // 疯狂循环，专门负责搬运音频数据
            driver->audio.loop();

            // 给看门狗一点喘息时间，但不能太长，1ms 足矣
            // 如果觉得还卡，可以试着把这一行注释掉
            vTaskDelay(1);
        }
    }

public:
    Audio audio;
    bool isPlaying = false;

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
        // 🚀 核心修改：创建独立任务
        // ---------------------------------------------------------
        // 参数说明：任务函数, 任务名, 栈大小(4K), 参数(this), 优先级(最高), 句柄, 核心(0)
        xTaskCreatePinnedToCore(
            audioTask,   // 任务函数
            "AudioTask", // 任务名
            4096,        // 栈大小 (4KB足够)
            this,        // 把自己传进去
            20,          // 优先级 (设高一点，比 LVGL 高)
            NULL,        // 句柄
            0            // 运行在核心 0 (LVGL 在核心 1)
        );

        Serial.println("[Audio] Running on Core 0 (Separate Thread)");
    }

    void play(const char *filename)
    {
        if (!SD_MMC.exists(filename))
            return;
        audio.connecttoFS(SD_MMC, filename);
        isPlaying = true;
    }

    // 主循环里其实不需要做事了，因为 Core 0 的任务在跑
    // 但保留这个函数接口，防止 main.cpp 报错
    void loop()
    {
        // 这里留空，千万不要再调 audio.loop() 了，否则双核打架会崩！
        if (isPlaying && !audio.isRunning())
            isPlaying = false;
    }

    void setVolume(uint8_t vol)
    {
        if (vol > 21)
            vol = 21;
        audio.setVolume(vol);
        uint8_t chip_vol = map(vol, 0, 21, 0, 255);
        writeReg(0x32, chip_vol);
    }

    void stop()
    {
        audio.stopSong();
        isPlaying = false;
    }
};

Audio_Driver audioDriver;