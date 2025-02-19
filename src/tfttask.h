#ifndef __TFT_TASK__
#define __TFT_TASK__

#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#include "ScopedMutex.h"
#include "tftMessage_t.h"

extern const char *PROGRAM_NAME;
extern SemaphoreHandle_t spiMutex;
QueueHandle_t tftQueue = NULL;

#endif
