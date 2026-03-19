#include "task_webserver.h"
#include <WiFi.h>

namespace {
struct WebRuntimeContext {
    AsyncWebServer server;
    AsyncWebSocket ws;
    SystemData_t *pLocalData;
    bool webserverIsRunning;

    WebRuntimeContext() : server(80), ws("/ws"), pLocalData(nullptr), webserverIsRunning(false) {}
};

WebRuntimeContext& getWebCtx() {
    static WebRuntimeContext ctx;
    return ctx;
}
}

void Webserver_sendata(String data)
{
    WebRuntimeContext &ctx = getWebCtx();
    if (ctx.ws.count() > 0)
    {
        ctx.ws.textAll(data);
        Serial.println("WebSocket sent: " + data);
    }
    else
    {
        Serial.println("No WebSocket clients connected");
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    WebRuntimeContext &ctx = getWebCtx();

    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;

        if (info->opcode == WS_TEXT)
        {
            String message;
            message += String((char *)data).substring(0, len);
            handleWebSocketMessage(message, ctx.pLocalData);
        }
    }
}

void connnectWSV(void *pvParameters)
{
    WebRuntimeContext &ctx = getWebCtx();
    ctx.pLocalData = (SystemData_t *)pvParameters;

    ctx.ws.onEvent(onEvent);
    ctx.server.addHandler(&ctx.ws);
    ctx.server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });
    ctx.server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/script.js", "application/javascript"); });
    ctx.server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/styles.css", "text/css"); });
    ctx.server.begin();
    ElegantOTA.begin(&ctx.server);
    ctx.webserverIsRunning = true;
    Serial.println("Web Server started and serving UI from LittleFS");

    while (1) {
        ctx.ws.cleanupClients();
        ElegantOTA.loop();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void vTaskWebUpdateDashboard(void *pvParameters) {
    SystemData_t *pData = (SystemData_t *)pvParameters;
    char jsonBuffer[384];

    while (1) {
        float t = 0, h = 0;
        bool l1 = false, l2 = false;
        uint32_t uptime = millis() / 1000UL;
        uint32_t heap = ESP.getFreeHeap();
        int32_t rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;

        if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY)) {
            t = pData->temperature;
            h = pData->humidity;
            l1 = pData->led1_status;
            l2 = pData->led2_status;
            xSemaphoreGive(pData->xDataMutex);
        }

        sprintf(
            jsonBuffer,
            "{\"temp\":%.1f,\"humi\":%.1f,\"led1\":%d,\"led2\":%d,\"uptime\":%lu,\"heap\":%lu,\"rssi\":%ld}",
            t,
            h,
            l1 ? 1 : 0,
            l2 ? 1 : 0,
            (unsigned long)uptime,
            (unsigned long)heap,
            (long)rssi
        );

        Webserver_sendata(String(jsonBuffer));

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void Webserver_stop()
{
    WebRuntimeContext &ctx = getWebCtx();
    ctx.ws.closeAll();
    ctx.server.end();
    ctx.webserverIsRunning = false;
}

void Webserver_reconnect() {
    WebRuntimeContext &ctx = getWebCtx();
    if (!ctx.webserverIsRunning && ctx.pLocalData != nullptr) {
        connnectWSV(ctx.pLocalData);
    }
    ElegantOTA.loop();
}