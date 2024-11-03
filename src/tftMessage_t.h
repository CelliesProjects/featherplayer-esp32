#ifndef _TFT_MESSAGE_
#define _TFT_MESSAGE_

#include <Arduino.h>

struct tftMessage
{
    enum Type
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
    Type type;
    char str[160];
    size_t value1 = 0;
    size_t value2 = 0;
};

#endif
