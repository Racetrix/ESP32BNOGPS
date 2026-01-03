#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLECharacteristic.h>

// 1. 定义回调函数类型
typedef void (*BLERecvCallback)(String);

// 2. 全局静态变量 (解决作用域问题)
// 放在最上面，这样回调类和驱动类都能访问，不会报错
static BLERecvCallback _onDataRecv = NULL;
static volatile bool _ble_connected = false;

// 3. [新增] 专门处理连接状态的回调类 (继承 ServerCallbacks)
class MyServerCallbacks : public NimBLEServerCallbacks
{
    void onConnect(NimBLEServer *pServer)
    {
        _ble_connected = true; // 修改静态变量
        Serial.println("[BLE] Device Connected");
    };

    void onDisconnect(NimBLEServer *pServer)
    {
        _ble_connected = false; // 修改静态变量
        Serial.println("[BLE] Device Disconnected");

        // 关键：断开后立即重新广播，方便手机重连
        // 不重置系统状态，不退出赛道模式
        NimBLEDevice::startAdvertising();
    }
};

// 4. [修改] 专门处理数据接收的回调类 (继承 CharacteristicCallbacks)
class RxCallbacks : public NimBLECharacteristicCallbacks
{
    void onWrite(NimBLECharacteristic *pCharacteristic)
    {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0)
        {
            if (_onDataRecv != NULL)
            {
                // 转成 Arduino String 传出去
                _onDataRecv(String(rxValue.c_str()));
            }
        }
    }
};

class BLE_Driver
{
private:
    NimBLEServer *pServer;
    NimBLEService *pService;
    NimBLECharacteristic *pTxCharacteristic;
    NimBLECharacteristic *pRxCharacteristic;

public:
    // 发送锁 (防止心跳包打断长数据)
    volatile bool isTxBusy = false;

    void init(String deviceName, BLERecvCallback cb = NULL)
    {
        if (cb != NULL)
        {
            _onDataRecv = cb;
        }

        NimBLEDevice::init(deviceName.c_str());
        pServer = NimBLEDevice::createServer();

        // [修复] 注册连接回调 (取消注释)
        pServer->setCallbacks(new MyServerCallbacks());

        pService = pServer->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");

        // Tx (Notify)
        pTxCharacteristic = pService->createCharacteristic(
            "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
            NIMBLE_PROPERTY::NOTIFY);

        // Rx (Write)
        pRxCharacteristic = pService->createCharacteristic(
            "6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
            NIMBLE_PROPERTY::WRITE);

        pRxCharacteristic->setCallbacks(new RxCallbacks());

        pService->start();
        NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
        pAdvertising->addServiceUUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
        pAdvertising->setScanResponse(true);
        pAdvertising->start();

        Serial.println("BLE Started. Waiting for connections...");
    }

    // 停止/开始心跳包发送的辅助函数
    void stopHealthPack()
    {
        isTxBusy = true;
    }
    void startHealthPack()
    {
        isTxBusy = false;
    }

    void send(std::string text)
    {
        // 使用静态变量判断连接
        if (_ble_connected)
        {
            pTxCharacteristic->setValue(text);
            pTxCharacteristic->notify();
        }
    }

    // 重载 1: 支持 Arduino String
    void send(String text)
    {
        send(std::string(text.c_str()));
    }

    // 重载 2: 支持 C 风格字符串
    void send(const char *text)
    {
        send(std::string(text));
    }

    // [修改] 获取连接状态
    bool isConnected()
    {
        return _ble_connected;
    }

    void printf(const char *format, ...)
    {
        char loc_buf[64];
        char *temp = loc_buf;
        va_list arg;
        va_list copy;
        va_start(arg, format);
        va_copy(copy, arg);
        int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
        va_end(copy);
        if (len < 0)
        {
            va_end(arg);
            return;
        }
        if (len >= sizeof(loc_buf))
        {
            temp = (char *)malloc(len + 1);
            if (temp == NULL)
            {
                va_end(arg);
                return;
            }
            len = vsnprintf(temp, len + 1, format, arg);
        }
        va_end(arg);
        send(temp);
        if (temp != loc_buf)
        {
            free(temp);
        }
    }
};

extern BLE_Driver ble;