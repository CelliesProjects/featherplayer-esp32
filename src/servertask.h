#ifndef __SERVER_TASK__
#define __SERVER_TASK__

#include <FFat.h>
#include <PsychicHttp.h>
#include <ESP32_VS1053_Stream.h>

#include "serverMessage_t.h"
#include "playerMessage_t.h"
#include "playList.h"
#include "index_htm_gz.h"

PsychicHttpServer server;
PsychicWebSocketHandler websocketHandler;
size_t _savedPosition = 0;

static char showStation[150]{};
static char streamTitle[150]{};

extern void sendPlayerMessage(playerMessage::Type type, uint8_t value = 0, size_t offset = 0);
extern String percentEncode(const char *plaintext);
extern void playListEnd();

extern const char *FAVORITES_FOLDER;
extern uint8_t _playerVolume;
extern playList_t playList;
extern QueueHandle_t serverQueue;
extern QueueHandle_t playerQueue;
extern bool _paused;

// icons used in webif
extern const char *radioicon;
extern const char *playicon;
extern const char *libraryicon;
extern const char *favoriteicon;
extern const char *pasteicon;
extern const char *deleteicon;
extern const char *addfoldericon;
extern const char *emptyicon;
extern const char *starticon;
extern const char *pauseicon;
extern const char *searchicon;
extern const char *nosslicon;

#endif
