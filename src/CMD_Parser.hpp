#pragma once
#include <Arduino.h>
#include "System_Config.hpp"
#include "IMU_Driver.hpp"
#include "Audio_Driver.hpp"
#include "BLE_Driver.hpp"
#include "GPS_Driver.hpp"
#include "Track_Manager.hpp"
#include "APP_UI.hpp" // 确保能访问 ui_ScreenMain
// 或者如果引用链太复杂，至少要加上这一行：
extern lv_obj_t *ui_ScreenMain;
extern lv_obj_t *ui_ScreenMode; // 如果有停止命令，可能需要切回来

extern GPS_Driver gps;
extern bool sd_connected;



class CommandParser
{
public:
    // 处理接收到的字符串
    void parse(String input)
    {
        input.trim(); // 去掉首尾空格和换行
        if (input.length() == 0)
            return;

        Serial.print("[CMD] Recv: ");
        Serial.println(input);

        // 1. 处理 SET 指令 (设置参数)
        // 格式: SET:VOL=50
        if (input.startsWith("SET:"))
        {
            int splitIndex = input.indexOf('=');
            if (splitIndex == -1)
                return; // 格式错误

            String key = input.substring(4, splitIndex);
            String valStr = input.substring(splitIndex + 1);
            int val = valStr.toInt(); // 转成数字

            if (key == "VOL")
            {
                sys_cfg.volume = constrain(val, 0, 21); // 限制范围
                audioDriver.setVolume(sys_cfg.volume);
                ble.send("OK:VOL=" + String(sys_cfg.volume));
            }
            else if (key == "SWAP")
            {
                sys_cfg.imu_swap_axis = (val == 1);
                imu.applyConfig(); // 立即生效
                ble.send("OK:SWAP=" + String(val));
            }
            else if (key == "INV_X")
            {
                sys_cfg.imu_invert_x = (val == 1);
                imu.applyConfig();
                ble.send("OK:INV_X=" + String(val));
            }
            else if (key == "INV_Y")
            {
                sys_cfg.imu_invert_y = (val == 1);
                imu.applyConfig();
                ble.send("OK:INV_Y=" + String(val));
            }
            else if (key == "GPS10")
            {
                sys_cfg.gps_10hz_mode = (val == 1);
                ble.send("OK:GPS10=" + String(val));
                // 注意：GPS设置可能需要重启或发送指令给GPS模块才能生效
            }
            else
            {
                ble.send("ERR:Unknown Key");
            }
        }

        // 2. 处理 CMD 指令 (执行动作)
        // 格式: CMD:SAVE
        else if (input.startsWith("CMD:"))
        {
            String action = input.substring(4);

            if (action == "SAVE")
            {
                sys_cfg.save();
                ble.send("OK:SAVED");
            }
            else if (action == "CAL")
            {
                // 执行复杂的校准逻辑 (这里简化，实际最好调用 Settings_UI 里的逻辑或封装 IMU 方法)
                // 这里我们假设我们在 IMU Driver 里加一个 calibrateZero() 方法
                // 或者简单的触发标志位
                ble.send("MSG:Calibrating...");
                // imu.calibrateZero(); // 你需要在 IMU Driver 里实现这个
                // 目前先模拟
                delay(1000);
                ble.send("OK:CAL_DONE");
            }
            else if (action == "SYNC")
            {
                // 手机刚连上时，把所有当前状态发给手机，以便同步 UI
                reportStatus();
            }
            else if(action == "REPORT"){
                reportHardwareStatus();
            }

        }
        else if (input.startsWith("RM:"))
        {
            String cmd = input.substring(3); // 去掉 "RM:"

            if (cmd == "START")
            {
                // 1. 安全检查：如果没有 GPS 定位，是否允许强行开始？
                // 建议：为了防止录制空数据，检查一下 GPS
                if (!gps.tgps.location.isValid())
                {
                    ble.send("ERR:GPS_NO_FIX");
                    Serial.println("[CMD] GPS not fixed, aborting.");
                    return;
                }

                // 2. 修改系统状态
                sys_cfg.current_mode = MODE_ROAM;
                sys_cfg.session_start_ms = millis(); // 记录开始时间
                sys_cfg.is_running = true;           // 这会触发 DataLogger 开始录制

                // 3. [关键] UI 切换到仪表盘
                // 就像用户点击了屏幕一样，自动跳到主界面
                if (ui_ScreenMain != NULL)
                {
                    lv_scr_load_anim(ui_ScreenMain, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
                }

                // 4. 回复手机
                ble.send("OK:RM_STARTED");
            }
            else if (cmd == "STOP")
            {
                // 停止录制
                sys_cfg.is_running = false;

                // 可选：停止后是否要自动切回菜单页？
                // if (ui_ScreenMode != NULL) {
                //    lv_scr_load_anim(ui_ScreenMode, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
                // }

                ble.send("OK:RM_STOPPED");
            }
        }
        else if (input.startsWith("TRACK:"))
        {
            String cmd = input.substring(6); // 去掉 "TRACK:" 前缀

            // 指令 A: 设置赛道参数
            // 格式: TRACK:SETUP=Type,Radius,StartLat,StartLon,EndLat,EndLon
            if (cmd.startsWith("SETUP="))
            {
                String params = cmd.substring(6); // 去掉 "SETUP="

                // 解析 6 个参数
                // 使用 double 保证经纬度精度 (float 在 ESP32 上只有 7 位有效数字，不够精确)
                double p[6] = {0};
                int pIndex = 0;
                int lastIndex = 0;

                for (int i = 0; i <= params.length(); i++)
                {
                    // 检测逗号或字符串结束
                    if (i == params.length() || params.charAt(i) == ',')
                    {
                        if (pIndex < 6)
                        {
                            String val = params.substring(lastIndex, i);
                            // 注意：ESP32 的 String.toDouble() 精度比 toFloat 高
                            p[pIndex] = val.toDouble();
                            pIndex++;
                        }
                        lastIndex = i + 1;
                    }
                }

                if (pIndex >= 4) // 至少需要 4 个参数 (模式, 半径, 起点Lat, 起点Lon)
                {
                    // 强制类型转换
                    TrackType mode = (TrackType)((int)p[0]);
                    float radius = (float)p[1];
                    double sLat = p[2];
                    double sLon = p[3];
                    double eLat = p[4];
                    double eLon = p[5];

                    // 执行设置
                    trackMgr.setupTrack(mode, radius, sLat, sLon, eLat, eLon);

                    // 回复手机
                    ble.send("OK:TRACK_UPDATED");
                }
                else
                {
                    ble.send("ERR:TRACK_ARGS");
                }
            }

            // 指令 B: 重置比赛 (用户手动点“重置”按钮)
            // 格式: TRACK:RESET
            else if (cmd == "RESET")
            {
                trackMgr.resetSession();
                ble.send("OK:TRACK_RESET");
            }

            else
            {
                ble.send("ERR:UNKNOWN_TRACK_CMD");
            }
        }
    }

    void reportHardwareStatus()
    {
        Serial.println("Reporting hardware status...");
        ble.send("SYS:REPORT_START");
        delay(20);

        // 1. SD 卡状态
        ble.send("SYS:SD=" + String(sd_connected ? 1 : 0));
        delay(20);

        // 2. IMU 状态 (通过 isConnected 标志)
        ble.send("SYS:IMU=" + String(imu.isConnected ? 1 : 0));
        delay(20);

        // 3. 电池电压 (假设有一个读取函数，这里模拟)
        float bat = analogRead(9) * 2.0 * 3.3 / 4095.0; // 简单模拟
        ble.send("SYS:BAT=" + String(bat, 2));
        delay(20);

        ble.send("SYS:REPORT_END");
    }

    // --- [新增] 发送心跳/遥测包 (高频) ---
    void sendTelemetry()
    {
        // [安全检查] 如果蓝牙未连接 或 正在发送长数据(Sync/Report)，则不发送心跳
        if (!ble.isConnected() || ble.isTxBusy)
            return;

        // 格式: TLM:速度,卫星,运行状态,模式类型,本圈时间/漫游时间,纬度,经度
        String packet = "TLM:";
        packet += String(gps.getSpeed(), 1); // 速度 (保留1位小数)
        packet += ",";
        packet += String(gps.getSatellites()); // 卫星数
        packet += ",";
        packet += sys_cfg.is_running ? "1" : "0"; // 是否开始
        packet += ",";

        // --- 模式类型 ---
        // -1: 漫游, 0: 圈赛, 1: 点对点
        if (trackMgr.isTrackSetup())
        {
            packet += String(trackMgr.getCurrentTrackType());
        }
        else
        {
            packet += "-1";
        }

        packet += ",";

        // --- [修复逻辑] 时间数据 ---
        if (sys_cfg.is_running)
        {
            if (trackMgr.isTrackSetup())
            {
                // 赛道模式：发送本圈已用时间 (由 TrackManager 计算，包含过线重置逻辑)
                packet += String(trackMgr.getCurrentLapElapsed());
            }
            else
            {
                // [修复] 漫游模式：发送总运行时间 (当前时间 - 开始时间)
                packet += String(millis() - sys_cfg.session_start_ms);
            }
        }
        else
        {
            // 未运行时发送 0 或 -1，手机端显示 00:00
            packet += "0";
        }

        // --- [新增] 经纬度信息 ---
        // 必须保留 6 位小数，否则地图上位置会乱跳
        packet += ",";
        packet += String(gps.tgps.location.lat(), 6);
        packet += ",";
        packet += String(gps.tgps.location.lng(), 6);

        ble.send(packet.c_str());
    }
    // 向手机汇报当前所有状态
    // 向手机汇报当前所有状态 (流式发送)
    void reportStatus()
    {
        ble.stopHealthPack();
        // 1. 告诉 APP 开始同步了
        ble.send("SYNC:START");
        delay(20); // 给蓝牙协议栈一点时间处理发送，防拥堵

        // 2. 逐条发送 (每条都很短，绝对安全)
        ble.send("VOL:" + String(sys_cfg.volume));
        delay(10);

        ble.send("SWAP:" + String(sys_cfg.imu_swap_axis));
        delay(10);

        ble.send("INV_X:" + String(sys_cfg.imu_invert_x));
        delay(10);

        ble.send("INV_Y:" + String(sys_cfg.imu_invert_y));
        delay(10);

        ble.send("GPS10:" + String(sys_cfg.gps_10hz_mode));
        delay(10);

        // 3. 以后这里加几十个都没问题，只是同步时间变长几百毫秒而已

        // 4. 结束标志
        ble.send("SYNC:END");
        delay(10);
        ble.startHealthPack();
    }
};

CommandParser cmdParser;