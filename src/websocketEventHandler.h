#ifndef __WEBSOCKET_HANDLERS__
#define __WEBSOCKET_HANDLERS__

#include <FFat.h>
#include <AsyncTCP.h>          /* use the esphome.io fork*/
#include <ESPAsyncWebServer.h> /* use the esphome.io fork*/

#include "playertask.h"

extern bool _paused;
extern playList_t playList;
extern void playListEnd();
extern const char *FAVORITES_FOLDER;

void websocketEventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void handleSingleFrame(AsyncWebSocketClient *client, uint8_t *data, size_t len);
void handleMultiFrame(AsyncWebSocketClient *client, uint8_t *data, size_t len, AwsFrameInfo *info);

#endif