#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>

#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// 1. 定义接收回调类 (处理手机发来的数据)
class RxCallbacks : public NimBLECharacteristicCallbacks
{
    NimBLECharacteristic *_pTxCharacteristic;

public:
    // 构造函数传入 TX 特征的指针，方便我们回传数据
    RxCallbacks(NimBLECharacteristic *pTx) : _pTxCharacteristic(pTx) {}

    // 当手机写入数据时触发
    void onWrite(NimBLECharacteristic *pCharacteristic) override
    {
        std::string rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0)
        {
            // A. 在串口打印收到的内容 (调试用)
            Serial.print("[BLE RX]: ");
            Serial.println(rxValue.c_str());

            // B. 回声逻辑：把收到的数据原样发回去
            if (_pTxCharacteristic)
            {
                // 加上前缀 "Echo: " 让你知道这是回传的
                std::string echoStr = "Echo: " + rxValue + "\n";
                _pTxCharacteristic->setValue((uint8_t *)echoStr.c_str(), echoStr.length());
                _pTxCharacteristic->notify();
            }
        }
    }
};

class BLE_Driver : public NimBLEServerCallbacks
{
private:
    NimBLEServer *pServer = NULL;
    NimBLECharacteristic *pTxCharacteristic = NULL;
    bool deviceConnected = false;

public:
    void init(String deviceName)
    {
        NimBLEDevice::init(deviceName.c_str());
        NimBLEDevice::setPower(ESP_PWR_LVL_P9);

        pServer = NimBLEDevice::createServer();
        pServer->setCallbacks(this);

        NimBLEService *pService = pServer->createService(SERVICE_UUID);

        // 创建 TX (发送)
        pTxCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID_TX,
            NIMBLE_PROPERTY::NOTIFY);

        // 创建 RX (接收)
        NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID_RX,
            NIMBLE_PROPERTY::WRITE);

        // 【关键】设置 RX 回调，把 TX 指针传进去以便回传
        pRxCharacteristic->setCallbacks(new RxCallbacks(pTxCharacteristic));

        pService->start();

        NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
        pAdvertising->addServiceUUID(SERVICE_UUID);
        pAdvertising->setScanResponse(true);
        pAdvertising->start();

        Serial.println("[BLE] Echo Service Started!");
    }

    // 主动发送函数 (给 task_logging 用)
    void printf(const char *format, ...)
    {
        if (!deviceConnected || !pTxCharacteristic)
            return;

        char loc_buf[64];
        char *temp = loc_buf;
        va_list arg;
        va_list copy;
        va_start(arg, format);
        va_copy(copy, arg);
        int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
        va_end(copy);

        if (len >= sizeof(loc_buf))
        {
            temp = new char[len + 1];
            vsnprintf(temp, len + 1, format, arg);
        }
        va_end(arg);

        pTxCharacteristic->setValue((uint8_t *)temp, len);
        pTxCharacteristic->notify();

        if (len >= sizeof(loc_buf))
        {
            delete[] temp;
        }
    }

    void onConnect(NimBLEServer *pServer)
    {
        deviceConnected = true;
        Serial.println("[BLE] Connected!");
    };

    void onDisconnect(NimBLEServer *pServer)
    {
        deviceConnected = false;
        Serial.println("[BLE] Disconnected.");
        NimBLEDevice::startAdvertising();
    }

    bool isConnected() { return deviceConnected; }
};

BLE_Driver ble;