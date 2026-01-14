#include "Audio_Driver.hpp"
extern Audio_Driver audioDriver;
#include <Arduino.h>

// 定义语音播报数据结构
struct LapVoiceData
{
    uint8_t minutes;       // 分钟数 (0-255)
    uint8_t seconds;       // 秒数 (0-59)
    uint8_t ms_tenths;     // 毫秒第一位 (0.x) -> "4"
    uint8_t ms_hundredths; // 毫秒第二位 (0.0x) -> "5"
    bool has_minutes;      // 是否包含分钟 (用于判断是否只播报秒)
};

LapVoiceData getLapVoiceData(uint32_t total_ms)
{
    LapVoiceData data;

    // 1. 计算分钟
    // 60000 ms = 1 minute
    data.minutes = (uint8_t)(total_ms / 60000);

    // 标记是否有分钟（如果小于1分钟，通常只报 "XX秒XX"）
    data.has_minutes = (data.minutes > 0);

    // 2. 计算剩余的毫秒数
    uint32_t remaining_ms = total_ms % 60000;

    // 3. 计算秒
    data.seconds = (uint8_t)(remaining_ms / 1000);

    // 4. 计算毫秒小数部分
    // remaining_ms % 1000 得到剩下的毫秒 (例如 456ms)
    // / 10 将 456 变成 45 (我们通常只报前两位，即百分之一秒)
    uint8_t fractional = (uint8_t)((remaining_ms % 1000) / 10);

    // 拆分小数位，方便语音逐字读出 "四" "五"
    data.ms_tenths = fractional / 10;     // 十位 (4)
    data.ms_hundredths = fractional % 10; // 个位 (5)

    return data;
}
void playMp3(String filename)
{
    // 方法：创建一个新的 String 对象来存储完整路径
    // "/mp3/num/" 会自动被转换成 String 并与 filename 拼接
    String fullPath = "/mp3/num/" + filename;

    // 然后将拼接好的结果转为 c_str() 传给驱动
    audioDriver.play(fullPath.c_str());
}

// --- 辅助函数：用 0-9 和 10 拼读 0-99 的整数 ---
void playNumberCN(uint8_t num)
{
    if (num <= 10)
    {
        // 情况 1: 0-10，直接播放
        playMp3(String(num) + ".mp3");
    }
    else if (num < 20)
    {
        // 情况 2: 11-19 -> "十" + "X"
        playMp3("10.mp3");                  // 播放 "十"
        playMp3(String(num - 10) + ".mp3"); // 播放 "个位"
    }
    else
    {
        // 情况 3: 20-99
        uint8_t ten = num / 10;
        uint8_t unit = num % 10;

        playMp3(String(ten) + ".mp3"); // 播放 "十位" (例如 35 的 3)
        playMp3("10.mp3");             // 播放 "十"

        if (unit > 0)
        {
            playMp3(String(unit) + ".mp3"); // 播放 "个位" (例如 35 的 5)
        }
    }
}

// --- 主函数：播放圈速 ---
void playLapRecord(uint32_t lap_ms)
{
    // 1. 调用你 hpp 里的计算函数，获取结构体
    LapVoiceData voice = getLapVoiceData(lap_ms);

    // 2. 播报分钟 (如果有)
    if (voice.has_minutes)
    {
        playNumberCN(voice.minutes); // 使用拼读函数 (例如 "1" 或 "12")
        playMp3("min.mp3");          // 播放 "分"
    }

    // 3. 播报秒
    // 逻辑优化：如果是整分（例如 1分05秒），中间的 0 要读出来吗？
    // 赛车通常习惯：
    //  - 1分12秒 -> "一" "分" "十" "二" "秒"
    //  - 1分05秒 -> "一" "分" "零" "五" "秒" (这时候要处理零)

    // 这里做一个简单的处理：如果前面有分钟，且秒数<10，补一个"0"的音
    if (voice.has_minutes && voice.seconds < 10)
    {
        playMp3("0.mp3");                        // 播放 "零"
        playMp3(String(voice.seconds) + ".mp3"); // 播放 "5"
    }
    else
    {
        playNumberCN(voice.seconds); // 正常拼读 "32" -> "三" "十" "二"
    }

    // 播放单位 "秒" (赛场上为了快节奏，有时候会省略这个字，看你喜好)
    playMp3("sec.mp3");

    // 4. 播报毫秒小数 (直接读数字，不拼读)
    // 例如 .45 读作 "四" "五"，而不是 "四十五"
    playMp3(String(voice.ms_tenths) + ".mp3");     // 播放 "4"
    playMp3(String(voice.ms_hundredths) + ".mp3"); // 播放 "5"
}