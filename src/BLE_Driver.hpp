#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLECharacteristic.h>
#include <TinyGPS++.h> // 必须引入，用于解析 GPS 对象

// ==========================================
// 1. 模式定义
// ==========================================
enum BLERunMode
{
    BLE_MODE_APP = 0,       // 你的 APP (NUS 串口模式)
    BLE_MODE_RACECHRONO = 1 // RaceChrono (原生二进制模式)
};

// ==========================================
// 2. UUID 定义
// ==========================================
// [你的 APP] NUS Service
#define UUID_APP_SERVICE "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define UUID_APP_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define UUID_APP_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// [RaceChrono] Native Binary Service (0x1FF8)
#define UUID_RC_SERVICE "00001ff8-0000-1000-8000-00805f9b34fb"
// Main GPS Data (0x0003)
#define UUID_RC_MAIN "00000003-0000-1000-8000-00805f9b34fb"
// Time Data (0x0004)
#define UUID_RC_TIME "00000004-0000-1000-8000-00805f9b34fb"

// ==========================================
// 3. RaceChrono 二进制数据包结构 (1字节对齐)
// ==========================================
#pragma pack(push, 1)
struct RaceChronoGPSPacket
{
    uint8_t time_0;
    uint8_t time_1;
    uint8_t time_2;
    uint8_t fix_satellites;
    int32_t latitude;
    int32_t longitude;
    uint16_t altitude;
    uint16_t speed;
    uint16_t bearing;
    uint8_t hdop;
    uint8_t vdop;
};

struct RaceChronoDatePacket
{
    uint8_t date_0;
    uint8_t date_1;
    uint8_t date_2;
};
#pragma pack(pop)

// 回调定义
typedef void (*BLERecvCallback)(String);
static BLERecvCallback _onDataRecv = NULL;
static volatile bool _ble_connected = false;

// 连接回调
class MyServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer)
    {
        _ble_connected = true;
        // 请求极速连接间隔 (7.5ms - 15ms)
        pServer->updateConnParams(pServer->getPeerInfo(0).getConnHandle(), 6, 12, 0, 200);
        Serial.println("[BLE] Connected");
    }
    void onDisconnect(NimBLEServer *pServer)
    {
        _ble_connected = false;
        Serial.println("[BLE] Disconnected");
        NimBLEDevice::startAdvertising();
    }
};

// 接收回调
class RxCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pCharacteristic)
    {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0 && _onDataRecv != NULL)
        {
            _onDataRecv(String(rxValue.c_str()));
        }
    }
};

class BLE_Driver
{
private:
    NimBLEServer *pServer;
    NimBLEService *pService;

    // 特征值句柄
    NimBLECharacteristic *pTxCharacteristic; // APP TX
    NimBLECharacteristic *pRxCharacteristic; // APP RX
    NimBLECharacteristic *pTxMain;           // RC 0x0003
    NimBLECharacteristic *pTxTime;           // RC 0x0004

    BLERunMode _currentMode = BLE_MODE_APP;
    uint8_t _rc_sync_counter = 0; // RaceChrono 同步计数器

public:
    volatile bool isTxBusy = false;

    // --- 原始流控函数保持不变 ---
    void stopHealthPack() { isTxBusy = true; }
    void startHealthPack() { isTxBusy = false; }

    // --- 初始化 ---
    void init(String deviceName, BLERunMode mode, BLERecvCallback cb = NULL)
    {
        _currentMode = mode;
        if (cb != NULL)
            _onDataRecv = cb;

        NimBLEDevice::init(deviceName.c_str());
        NimBLEDevice::setMTU(185);

        pServer = NimBLEDevice::createServer();
        pServer->setCallbacks(new MyServerCallbacks());

        if (_currentMode == BLE_MODE_APP)
        {
            // === 模式 A: 你的 APP (NUS) ===
            pService = pServer->createService(UUID_APP_SERVICE);
            pTxCharacteristic = pService->createCharacteristic(UUID_APP_TX, NIMBLE_PROPERTY::NOTIFY);
            pRxCharacteristic = pService->createCharacteristic(UUID_APP_RX, NIMBLE_PROPERTY::WRITE);
            pRxCharacteristic->setCallbacks(new RxCallbacks());
            pService->start();

            NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
            pAdvertising->addServiceUUID(UUID_APP_SERVICE);
            pAdvertising->setScanResponse(true);
            pAdvertising->start();
            Serial.println("[BLE] Mode: APP (NUS)");
        }
        else
        {
            // === 模式 B: RaceChrono (Native Binary) ===
            pService = pServer->createService(UUID_RC_SERVICE);

            // 0x0003: Main GPS Data (20 Hz)
            pTxMain = pService->createCharacteristic(
                UUID_RC_MAIN, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

            // 0x0004: GPS Time Data (1 Hz)
            pTxTime = pService->createCharacteristic(
                UUID_RC_TIME, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

            pService->start();

            NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
            pAdvertising->addServiceUUID(UUID_RC_SERVICE);
            pAdvertising->setScanResponse(true);
            pAdvertising->start();
            Serial.println("[BLE] Mode: RaceChrono (Native Binary 0x0003)");
        }
    }

    // --- 发送给 APP (保持不变) ---
    void send(String text)
    {
        if (_ble_connected && _currentMode == BLE_MODE_APP)
        {
            pTxCharacteristic->setValue((uint8_t *)text.c_str(), text.length());
            pTxCharacteristic->notify();
        }
    }

    // --- [新增] 发送 RaceChrono 原生二进制数据 ---
    // 替代之前的 sendNMEA / sendRC3
    void sendRaceChronoBinary(TinyGPSPlus &gps)
    {
        if (!_ble_connected || _currentMode != BLE_MODE_RACECHRONO)
            return;
        if (!gps.location.isValid())
            return; // 没定位不发，节省带宽

        // 1. 更新 Sync Counter (3-bit, 0-7 循环)
        _rc_sync_counter = (_rc_sync_counter + 1) & 0x07;

        // ==========================================
        // 打包 0x0003 (Main Data - 20 Bytes)
        // ==========================================
        RaceChronoGPSPacket p;
        memset(&p, 0, sizeof(p));

        // Time: (min*30000 + sec*500 + ms/2) | (sync << 21)
        uint32_t t_field = (gps.time.minute() * 30000) + (gps.time.second() * 500) + (gps.time.centisecond() * 5);
        uint32_t t_comb = (t_field & 0x1FFFFF) | (_rc_sync_counter << 21);
        p.time_0 = t_comb & 0xFF;
        p.time_1 = (t_comb >> 8) & 0xFF;
        p.time_2 = (t_comb >> 16) & 0xFF;

        // Fix & Sats
        uint8_t sats = gps.satellites.value();
        if (sats > 63)
            sats = 63;
        p.fix_satellites = (1 << 6) | (sats & 0x3F); // Bit 6=1 (GPS Fix)

        // Lat / Lon (deg * 1e7)
        p.latitude = (int32_t)(gps.location.lat() * 10000000.0);
        p.longitude = (int32_t)(gps.location.lng() * 10000000.0);

        // Altitude (encoded)
        float alt = gps.altitude.meters();
        if (alt >= -500 && alt <= 6053.5)
            p.altitude = (uint16_t)((alt + 500) * 10) & 0x7FFF;
        else
            p.altitude = ((uint16_t)(alt + 500) & 0x7FFF) | 0x8000;

        // Speed (encoded)
        float spd = gps.speed.kmph();
        if (spd <= 655.35)
            p.speed = (uint16_t)(spd * 100) & 0x7FFF;
        else
            p.speed = ((uint16_t)(spd * 10) & 0x7FFF) | 0x8000;

        // Bearing (deg * 100)
        p.bearing = (uint16_t)(gps.course.deg() * 100);

        // DOP
        p.hdop = (uint8_t)(gps.hdop.value() / 10.0); // TinyGPS returns value*100, we need value*10
        p.vdop = 0xFF;

        // 发送主包
        pTxMain->setValue((uint8_t *)&p, sizeof(p));
        pTxMain->notify();

        // ==========================================
        // 打包 0x0004 (Time Data - 3 Bytes)
        // ==========================================
        // 实际上可以低频发送，但每次发也行，才3字节
        RaceChronoDatePacket d;
        uint32_t d_field = (gps.date.year() - 2000) * 8928 +
                           (gps.date.month() - 1) * 744 +
                           (gps.date.day() - 1) * 24 +
                           gps.time.hour();
        uint32_t d_comb = (d_field & 0x1FFFFF) | (_rc_sync_counter << 21);

        d.date_0 = d_comb & 0xFF;
        d.date_1 = (d_comb >> 8) & 0xFF;
        d.date_2 = (d_comb >> 16) & 0xFF;

        pTxTime->setValue((uint8_t *)&d, sizeof(d));
        pTxTime->notify();
    }

    // --- 兼容性保留 (防止你其他地方报错，但 RC 模式下不工作) ---
    void sendNMEA(uint8_t byte) { /* 原生协议模式下不再使用 */ }

    // --- 辅助 printf ---
    void printf(const char *format, ...)
    {
        char loc_buf[64];
        va_list args;
        va_start(args, format);
        vsnprintf(loc_buf, sizeof(loc_buf), format, args);
        va_end(args);
        send(String(loc_buf));
    }

    // 获取状态
    bool isConnected() { return _ble_connected; }
    BLERunMode getMode() { return _currentMode; }
};

extern BLE_Driver ble;