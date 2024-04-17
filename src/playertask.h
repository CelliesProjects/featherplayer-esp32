#ifndef __PLAYER_TASK__
#define __PLAYER_TASK__

#include <VS1053.h> /* https://github.com/baldram/ESP_VS1053_Library */
#include <ESP32_VS1053_Stream.h>

#include "tfttask.h"

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
        /*WS_UPDATE_STATION,
        WS_UPDATE_STREAMTITLE,
        WS_UPDATE_NOWPLAYING,
        WS_UPDATE_FAVORITES,
        WS_UPDATE_PLAYLIST,
        WS_PASS_MESSAGE*/
    };
    action action;
    char str[256];
    bool singleClient = false;
    size_t value = 0;
    size_t offset = 0;
};

void playerTask(void *parameter);
void playListEnd();

#endif