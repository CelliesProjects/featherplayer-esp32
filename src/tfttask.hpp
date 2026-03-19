#ifndef _TFTTASK_HPP_
#define _TFTTASK_HPP_

#include <WiFi.h>

#include "LGFX_AdaFruitTFT_1.14_ST7789.hpp"
#include "ScopedMutex.h"
#include "tftMessage_t.h"

extern const char *PROGRAM_NAME;
extern SemaphoreHandle_t spiMutex;
extern TaskHandle_t playerTaskHandle;

QueueHandle_t tftQueue = NULL;
char previousTime[CLOCKSTR_LEN] = "";
LGFX tft;
LGFX_Sprite canvas(&tft);
LGFX_Sprite loader(&tft);

char streamTitle[264];
int16_t streamTitleOffset = 0;
uint16_t streamTitleWidth = 0;
unsigned long lastTitleShow = 0;
bool clearTitle = false;
bool showClock = false;

const uint16_t BACKGROUND_COLOR = lgfx::color565(29, 29, 29);
const uint16_t TEXT_COLOR = lgfx::color565(245, 244, 226);

#endif
