
#ifndef __TASK_HANDLER_H__
#define __TASK_HANDLER_H__

#include <ArduinoJson.h>
#include <task_check_info.h>
#include "global.h"

void handleWebSocketMessage(String message, void *pvParameters);
bool applyDeviceState(SystemData_t *pData, int gpio, bool state);
bool applyLcdState(SystemData_t *pData, bool state);
bool applyFanSpeed(SystemData_t *pData, uint8_t speed);
#endif