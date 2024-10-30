#ifndef __PLAYER_TASK__
#define __PLAYER_TASK__

#include <VS1053.h> /* https://github.com/baldram/ESP_VS1053_Library */
#include <ESP32_VS1053_Stream.h>

#include "playerMessage_t.h"
#include "serverMessage_t.h"
#include "tftMessage_t.h"
#include "playList.h"

#define VS1053_CS 6
#define VS1053_DCS 10
#define VS1053_DREQ 9

extern const char *PROGRAM_NAME;
extern QueueHandle_t tftQueue;
extern QueueHandle_t serverQueue;
extern QueueHandle_t playerQueue;

extern playList_t playList;
extern SemaphoreHandle_t spiMutex;
extern uint8_t _playerVolume;
extern bool _paused;
extern size_t _savedPosition;

extern void sendServerMessage(serverMessage::Type type, const char *str = NULL, bool singleClient = false, size_t value = 0, size_t value2 = 0);
extern void sendTftMessage(tftMessage::Type type, const char *str = NULL, size_t value1 = 0, size_t value2 = 0);
#endif
