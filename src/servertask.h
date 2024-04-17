#ifndef __SERVER_TASK__
#define __SERVER_TASK__

#include <FFat.h>
#include <AsyncTCP.h>          /* use the esphome.io fork*/
#include <ESPAsyncWebServer.h> /* use the esphome.io fork*/

#include "percentEncode.h"
#include "playList.h"
#include "index_htm_gz.h"
#include "icons.h"

extern playList_t playList;
extern QueueHandle_t serverQueue;
extern void websocketEventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

struct serverMessage
{
    enum type
    {
        WS_UPDATE_STATION,
        WS_UPDATE_STREAMTITLE,
        WS_UPDATE_NOWPLAYING,
        WS_UPDATE_FAVORITES,
        WS_UPDATE_PLAYLIST,
        WS_PASS_MESSAGE
    };
    type type;
    size_t value;
    char str[32]{};
    bool singleClient = false;
};

void callbackSetup(AsyncWebServer &server);
void serverTask(void *parameter);

#endif
