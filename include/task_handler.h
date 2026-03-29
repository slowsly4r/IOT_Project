
#ifndef __TASK_HANDLER_H__
#define __TASK_HANDLER_H__

#include <ArduinoJson.h>
#include <task_check_info.h>
#include "global.h"

void handleWebSocketMessage(String message, void *pvParameters);
#endif