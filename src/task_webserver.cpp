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

void handleHttpAction(AsyncWebServerRequest *request)
{
    WebRuntimeContext &ctx = getWebCtx();

    if (ctx.pLocalData == nullptr) {
        request->send(503, "text/plain", "Device not ready");
        return;
    }

    if (!request->hasParam("status")) {
        request->send(400, "text/plain", "Missing status");
        return;
    }

    String status = request->getParam("status")->value();

    if (request->hasParam("device")) {
        String device = request->getParam("device")->value();
        if (device.equalsIgnoreCase("lcd")) {
            bool state = status.equalsIgnoreCase("ON");
            if (applyLcdState(ctx.pLocalData, state)) {
                request->send(200, "text/plain", "OK");
            } else {
                request->send(400, "text/plain", "Ignored");
            }
            return;
        }
    }

    if (!request->hasParam("gpio")) {
        request->send(400, "text/plain", "Missing gpio or device");
        return;
    }

    int gpio = request->getParam("gpio")->value().toInt();

    if (gpio == FAN_GPIO && status.equalsIgnoreCase("SPEED")) {
        if (!request->hasParam("value")) {
            request->send(400, "text/plain", "Missing value");
            return;
        }

        int speed = request->getParam("value")->value().toInt();
        speed = constrain(speed, 0, 255);
        if (applyFanSpeed(ctx.pLocalData, (uint8_t)speed)) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Ignored");
        }
        return;
    }

    if (gpio == 6) {
        bool state = status.equalsIgnoreCase("ON");
        if (applyLcdState(ctx.pLocalData, state)) {
            request->send(200, "text/plain", "OK");
        } else {
            request->send(400, "text/plain", "Ignored");
        }
        return;
    }

    bool state = status.equalsIgnoreCase("ON");

    if (applyDeviceState(ctx.pLocalData, gpio, state)) {
        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "Ignored");
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
            String message = String((char *)data, len);
            Serial.printf("WebSocket RX len=%u: %s\n", (unsigned)len, message.c_str());
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
    ctx.server.on("/action", HTTP_GET, handleHttpAction);
    ctx.server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });
    ctx.server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/script.js", "application/javascript"); });
    ctx.server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/styles.css", "text/css"); });
    ElegantOTA.begin(&ctx.server);
    ctx.server.begin();
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
        bool lcdState = false, fanState = false;
        int aiPrediction = 0;
        uint8_t fanSpeed = 0;
        uint32_t uptime = millis() / 1000UL;
        uint32_t heap = ESP.getFreeHeap();
        int32_t rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;

        if (xSemaphoreTake(pData->xDataMutex, portMAX_DELAY)) {
            t = pData->temperature;
            h = pData->humidity;
            lcdState = pData->lcd_status;
            fanState = pData->fan_status;
            aiPrediction = pData->ai_prediction;
            fanSpeed = pData->fan_speed;
            xSemaphoreGive(pData->xDataMutex);
        }

        sprintf(
            jsonBuffer,
            "{\"temp\":%.1f,\"humi\":%.1f,\"lcd\":%d,\"fan\":%d,\"ai\":%d,\"fanSpeed\":%u,\"uptime\":%lu,\"heap\":%lu,\"rssi\":%ld}",
            t,
            h,
            lcdState ? 1 : 0,
            fanState ? 1 : 0,
            aiPrediction,
            (unsigned int)fanSpeed,
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