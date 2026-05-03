#ifndef __TASK_LCD_DISPLAY_H__
#define __TASK_LCD_DISPLAY_H__

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "global.h"

void LcdInit();
void LcdSetPower(bool enabled);
bool LcdGetPower();
void vTaskLcdDisplay(void *pvParameters);

#endif