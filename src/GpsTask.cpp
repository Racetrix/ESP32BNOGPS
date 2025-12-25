#include "GpsTask.h"
#include "Globals.h"

#define GPS_RX 4
#define GPS_TX 5
#define GPS_BAUD 115200

HardwareSerial gpsSerial(1);
TinyGPSPlus gps;

// 全局变量实体定义
volatile double g_lat = 0, g_lon = 0;
volatile float g_spd = 0, g_alt = 0;
volatile int g_sat = 0;
volatile bool g_fix = false;

void TaskGPS_Code(void *pvParameters)
{
    gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

    while (true)
    {
        while (gpsSerial.available() > 0)
        {
            gps.encode(gpsSerial.read());
        }

        // 更新全局缓存
        if (gps.location.isUpdated() || gps.satellites.isUpdated())
        {
            g_lat = gps.location.lat();
            g_lon = gps.location.lng();
            g_spd = gps.speed.kmph();
            g_alt = gps.altitude.meters();
            g_sat = gps.satellites.value();
            g_fix = gps.location.isValid();
        }

        vTaskDelay(5 / portTICK_PERIOD_MS); // 防止看门狗咬人
    }
}

void initGpsTask()
{
    xTaskCreatePinnedToCore(TaskGPS_Code, "GPS_Core0", 4096, NULL, 1, NULL, 0);
}