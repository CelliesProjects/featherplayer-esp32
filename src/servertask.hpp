#ifndef _SERVERTASK_HPP_
#define _SERVERTASK_HPP_

#include <FFat.h>
#include <PsychicHttp.h>
#include <ESP32_VS1053_Stream.h>

#include "serverMessage_t.h"
#include "playerMessage_t.h"
#include "playList.h"

const char *FAVORITES_FOLDER = "/"; /* if this is a folder use a closing slash */

static PsychicHttpServer server;
static PsychicWebSocketHandler websocketHandler;

QueueHandle_t serverQueue = NULL;
size_t _savedPosition = 0;

static char showStation[190]{};
static char streamTitle[180]{};

extern QueueHandle_t playerQueue;
extern void sendPlayerMessage(playerMessage::Type type, uint8_t value = 0, size_t offset = 0);
extern String percentEncode(const char *plaintext);
extern void playListEnd();

extern uint8_t _playerVolume;
extern playList_t playList;
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
