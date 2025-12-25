#include "BtManager.h"
#include "BluetoothSerial.h"
#include "Globals.h"
#include "Protocol.h"
#include <Preferences.h> // 1. 引入首选项库

BluetoothSerial SerialBT;
Preferences preferences; // 2. 创建存储对象

// --- 全局变量 ---
volatile bool isRaceMode = false;
volatile bool isAuthPassed = false;

String secretKey;

// 内部变量
bool lastConnected = false;

void initBluetooth()
{
    SerialBT.begin("ESP32_Race_Master");
    Serial.println("Bluetooth Started! Waiting for pairing...");

    // 4. 初始化存储并读取密码
    // "race_config" 是命名空间，false 表示读写模式
    preferences.begin("race_config", false);

    // 尝试读取键名为 "bt_pass" 的值，如果读不到（第一次运行），默认返回 "1234"
    secretKey = preferences.getString("bt_pass", "1234");

    Serial.print("Current Password: ");
    Serial.println(secretKey);
}

void checkConnectionGuard()
{
    bool currentConnected = SerialBT.hasClient();

    if (lastConnected && !currentConnected)
    {
        Serial.println("⚠️ Bluetooth Disconnected! System Reset.");
        isRaceMode = false;
        isAuthPassed = false;
    }
    lastConnected = currentConnected;
}

void processCommand(String cmd)
{
    cmd.trim();
    if (cmd.length() == 0)
        return;

    Serial.print("RX CMD: ");
    Serial.println(cmd);

    // --- 1. 鉴权逻辑 (使用读取到的 secretKey) ---
    if (cmd.startsWith("KEY:"))
    {
        String key = cmd.substring(4);
        // 这里和动态变量 secretKey 比较
        if (key == secretKey)
        {
            isAuthPassed = true;
            SerialBT.println("MSG:Auth OK");
        }
        else
        {
            SerialBT.println("MSG:Auth Failed");
        }
        return;
    }

    if (!isAuthPassed)
    {
        SerialBT.println("ERR:Login First");
        return;
    }

    // --- 新增：修改密码指令 ---
    // 格式：CMD:SET_PASS:8888
    if (cmd.startsWith("CMD:SET_PASS:"))
    {
        String newPass = cmd.substring(13); // 截取指令后的内容
        newPass.trim();

        if (newPass.length() > 0 && newPass.length() <= 16)
        {
            secretKey = newPass;                         // 更新内存变量
            preferences.putString("bt_pass", secretKey); // 写入闪存 (永久保存)
            SerialBT.println("MSG:Password Updated");
            Serial.println("New Password Saved: " + secretKey);
        }
        else
        {
            SerialBT.println("ERR:Invalid Password");
        }
        return;
    }

    // --- 2. 模式切换逻辑 (保持不变) ---
    if (cmd == "CMD:RACE_ON")
    {
        isRaceMode = true;
        SerialBT.println("MSG:Race Mode ON");
    }
    else if (cmd == "CMD:RACE_OFF")
    {
        isRaceMode = false;
        SerialBT.println("MSG:Race Mode OFF");
    }
}

void loopBluetoothInput()
{
    checkConnectionGuard();
    if (SerialBT.available())
    {
        String line = SerialBT.readStringUntil('\n');
        processCommand(line);
    }
}

void sendRacePacket()
{
    if (!SerialBT.hasClient() || !isAuthPassed)
        return;

    SerialBT.printf("$RC,%d,%d,%.6f,%.6f,%.2f,%.2f\n",
                    g_fix ? 1 : 0,
                    g_sat,
                    g_lat,
                    g_lon,
                    g_spd,
                    g_alt);
}

void sendHeartbeatPacket()
{
    if (!SerialBT.hasClient() || !isAuthPassed)
        return;

    // 修改状态位计算：移除了 WiFi 的位 (2)
    // 0=Idle, 1=Race, 4=AuthPassed
    int status = (isRaceMode ? 1 : 0) | (isAuthPassed ? 4 : 0);

    SerialBT.printf("$HB,%d,%d,%d,%.2f\n",
                    status,
                    g_fix ? 1 : 0,
                    g_sat,
                    3.7);
}