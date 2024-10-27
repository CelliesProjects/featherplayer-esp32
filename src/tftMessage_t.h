#ifndef _TFT_MESSAGE_
#define _TFT_MESSAGE_

#include <Arduino.h>

struct tftMessage
{
    enum type
    {
        SYSTEM_MESSAGE,
        PROGRESS_BAR,
        CLEAR_SCREEN,
        SHOW_STATION,
        SHOW_TITLE,
        SHOW_CODEC,
        SHOW_IPADDRESS,
        SHOW_CLOCK,
        BUFFER_STATUS
    };
    type type;
    char str[256];
    size_t value1 = 0;
    size_t value2 = 0;
};

#endif