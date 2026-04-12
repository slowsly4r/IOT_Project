#ifndef __TASK_WIFI_H__
#define __TASK_WIFI_H__

#include <WiFi.h>
#include "global.h"
#include <task_check_info.h>
#include <task_webserver.h>

// #define SSID_AP "ESP32-LOCAL"
// #define PASS_AP "12345678"

bool Wifi_reconnect(SystemData_t *pData);
void startAP();
void startSTA(SystemData_t *pData);
void vTaskWifi(void *pvParameters);

#endif