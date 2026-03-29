#include "task_wifi.h"

void startAP()
{
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(String(SSID_AP), String(PASS_AP));
    Serial.print("AP Mode Started. IP: ");
    Serial.println(WiFi.softAPIP());
}

void startSTA(SystemData_t *pData)
{
    if (pData->wifi_ssid.isEmpty())
    {
        Serial.println("WiFi SSID is empty, skipping STA...");
        return;
    }

    Serial.println("Connecting to: " + pData->wifi_ssid);
    WiFi.mode(WIFI_AP_STA);

    if (pData->wifi_pass.isEmpty())
    {
        WiFi.begin(pData->wifi_ssid.c_str());
    }
    else
    {
        WiFi.begin(pData->wifi_ssid.c_str(), pData->wifi_pass.c_str());
    }

    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 20)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
        attempt++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        xSemaphoreGive(pData->xInternetReady);
    } else {
        Serial.println("\nWiFi Connection Failed!");
    }
}

bool Wifi_reconnect(SystemData_t *pData)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }
    
    startSTA(pData);
    return (WiFi.status() == WL_CONNECTED);
}

void vTaskWifi(void *pvParameters)
{
    SystemData_t *pData = (SystemData_t *)pvParameters;

    // 1. Khởi động lần đầu
    // Nếu chưa có cấu hình WiFi, phát AP để người dùng vào cài đặt
    startAP();

    if (!pData->wifi_ssid.isEmpty()) {
        startSTA(pData);
    }

    while (1)
    {
        // 2. Kiểm tra và tự động kết nối lại mỗi 30 giây
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi lost! Attempting to reconnect...");
            Wifi_reconnect(pData);
        }

        vTaskDelay(pdMS_TO_TICKS(30000)); // Nghỉ 30 giây rồi kiểm tra lại
    }
}
