#ifndef __PLAYER_TASK__
#define __PLAYER_TASK__

#include <VS1053.h> /* https://github.com/baldram/ESP_VS1053_Library */
#include <ESP32_VS1053_Stream.h>

#include "tfttask.h"
#include "playList.h"

#define VS1053_CS 6
#define VS1053_DCS 10
#define VS1053_DREQ 9
#define SDREADER_CS 5

extern playList_t playList;
extern SemaphoreHandle_t spiMutex;
extern QueueHandle_t playerQueue;
extern size_t _savedPosition;
extern uint8_t _playerVolume;
extern bool _paused;

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

void playerTask(void *parameter);
void playListEnd();

#endif