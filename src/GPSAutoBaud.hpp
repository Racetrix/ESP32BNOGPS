#pragma once
#include <Arduino.h>

class GPSAutoBaud
{
private:
    HardwareSerial *targetSerial;
    uint8_t _rxPin, _txPin;

    // 我们要探测的候选波特率列表
    // 优先尝试 115200 (期望值)，然后是 9600 (默认值)，最后是其他可能的
    const uint32_t CANDIDATE_BAUDS[4] = {115200, 9600, 38400, 57600};
    const uint8_t NUM_CANDIDATES = 4;

    // 每个波特率尝试监听的时间 (毫秒)
    // GPS 默认 1Hz (1秒1次)，所以至少要听 1100ms 才能保证抓到一包数据
    // 如果 GPS 已经是 10Hz，这个过程会很快
    const uint32_t LISTEN_TIMEOUT_MS = 1200;

public:
    GPSAutoBaud(HardwareSerial *serial, uint8_t rx, uint8_t tx)
        : targetSerial(serial), _rxPin(rx), _txPin(tx) {}

    /**
     * @brief 开始自动检测
     * @return 探测到的波特率，如果全失败返回 0
     */
    uint32_t detect()
    {
        Serial.println("[AutoBaud] Starting detection...");

        for (int i = 0; i < NUM_CANDIDATES; i++)
        {
            uint32_t baud = CANDIDATE_BAUDS[i];
            Serial.printf("[AutoBaud] Trying %lu... ", baud);

            // 1. 初始化串口
            targetSerial->begin(baud, SERIAL_8N1, _rxPin, _txPin);

            // 给一点时间让串口稳定
            delay(100);

            // 清空缓冲区，避免读取到上一次波特率遗留的乱码
            while (targetSerial->available())
                targetSerial->read();

            // 2. 监听数据
            uint32_t start_time = millis();
            bool found = false;
            uint8_t valid_char_count = 0;

            while (millis() - start_time < LISTEN_TIMEOUT_MS)
            {
                if (targetSerial->available())
                {
                    uint8_t c = targetSerial->read();

                    // 3. 核心判断逻辑：特征码匹配
                    // ---------------------------------------------
                    // 特征 A: NMEA 协议以 '$' (0x24) 开头
                    // 特征 B: U-blox UBX 协议以 0xB5 开头
                    // ---------------------------------------------
                    if (c == '$' || c == 0xB5)
                    {
                        // 发现疑似报头，进一步验证后续几个字节防止误判
                        // 简单的做法是：只要发现 '$' 基本上就是对的
                        found = true;
                        break;
                    }

                    // 备选逻辑：统计“看起来像文本”的字符
                    // 如果全是乱码（比如高位为1的字符），通常说明波特率错了
                    if (c >= 32 && c <= 126)
                    {
                        valid_char_count++;
                    }
                    else
                    {
                        valid_char_count = 0; // 遇到乱码重置
                    }

                    // 如果连续收到 20 个合法 ASCII 字符，也认为是对的
                    if (valid_char_count > 20)
                    {
                        found = true;
                        break;
                    }
                }
                yield(); // 防止看门狗复位
            }

            if (found)
            {
                Serial.println("SUCCESS! ✅");
                // 此时不要关闭串口，直接返回成功的波特率
                // 调用者可以直接继续使用，或者根据需要重新 begin
                return baud;
            }
            else
            {
                Serial.println("No valid data.");
                targetSerial->end(); // 关闭，准备下一次尝试
                delay(50);
            }
        }

        Serial.println("[AutoBaud] Failed to detect GPS.");
        return 0;
    }
};