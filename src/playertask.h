#ifndef __PLAYER_TASK__
#define __PLAYER_TASK__

#include <VS1053.h> /* https://github.com/baldram/ESP_VS1053_Library */
#include <ESP32_VS1053_Stream.h>

#include "playerMessage_t.h"
#include "servertask.h"
#include "tfttask.h"

#define VS1053_CS 6
#define VS1053_DCS 10
#define VS1053_DREQ 9
#define SDREADER_CS 5

extern playList_t playList;
extern SemaphoreHandle_t spiMutex;
extern QueueHandle_t playerQueue;
extern uint8_t _playerVolume;
extern bool _paused;
extern size_t _savedPosition;

void playerTask(void *parameter);
void playListEnd();

#endif