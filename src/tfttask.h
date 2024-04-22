#ifndef __TFT_TASK__
#define __TFT_TASK__

#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>

extern const char *PROGRAM_NAME;
extern SemaphoreHandle_t spiMutex;
extern QueueHandle_t tftQueue;

float map_range(const float input,
                const float input_start, const float input_end,
                const float output_start, const float output_end);

void tftTask(void *parameter);
 
struct tftMessage
{
    enum action
    {
        SYSTEM_MESSAGE,
        PROGRESS_BAR,
        CLEAR_SCREEN,
        SHOW_STATION,
        SHOW_TITLE,
        SHOW_CODEC,
        SHOW_IPADDRESS
    };
    action action;
    char str[256];
    size_t value1 = 0;
    size_t value2 = 0;
};

#endif