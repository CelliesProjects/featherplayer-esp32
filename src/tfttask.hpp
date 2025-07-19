#ifndef _TFTTASK_HPP_
#define _TFTTASK_HPP_

#include <WiFi.h>
#include "LGFX_AdaFruitTFT.hpp"

//#include <Fonts/FreeSansBold9pt7b.h>
//#include <Fonts/FreeSansBold18pt7b.h>
//#include <Fonts/FreeSansBold24pt7b.h>

#include "ScopedMutex.h"
#include "tftMessage_t.h"

extern const char *PROGRAM_NAME;
extern SemaphoreHandle_t spiMutex;
QueueHandle_t tftQueue = NULL;

#endif
