#ifndef __SERVER_TASK__
#define __SERVER_TASK__

#include <FFat.h>
#include <PsychicHttp.h>

#include "playList.h"
#include "index_htm_gz.h"

PsychicHttpServer server;
PsychicWebSocketHandler websocketHandler();

extern const char *FAVORITES_FOLDER;
extern uint8_t _playerVolume;
extern playList_t playList;
extern String percentEncode(const char *plaintext);
extern QueueHandle_t serverQueue;
extern void websocketEventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

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

struct serverMessage
{
    enum type
    {
        WS_UPDATE_STATION,
        WS_UPDATE_STREAMTITLE,
        WS_UPDATE_NOWPLAYING,
        WS_UPDATE_FAVORITES,
        WS_UPDATE_PLAYLIST,
        WS_UPDATE_VOLUME,
        WS_PASS_MESSAGE,
        WS_UPDATE_PROGRESS,
        WS_UPDATE_STATUS
    };
    type type;
    size_t value;
    size_t value2;
    char str[160]{};
    bool singleClient = false;
};

void callbackSetup(AsyncWebServer &server);
void serverTask(void *parameter);

#endif
