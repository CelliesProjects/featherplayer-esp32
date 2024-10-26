#ifndef _PLAYER_MESSAGE_
#define _PLAYER_MESSAGE_

#include <Arduino.h>

struct playerMessage
{
    enum action
    {
        SET_VOLUME,
        START_ITEM,
        STOPSONG,
        PAUSE,
        RESUME,
        SETTONE,
    };
    action action;
    char str[256];
    bool singleClient = false;
    size_t value = 0;
    size_t offset = 0;
};

#endif