#ifndef __TASK_LED_TEMP_H__
#define __TASK_LED_TEMP_H__
#include <Arduino.h>
#include "global.h"
#define LED_GPIO 48

void vTaskLedTempControl(void *pvParameters);


#endif