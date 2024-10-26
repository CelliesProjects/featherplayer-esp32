#ifndef __WEBSOCKET_HANDLERS__
#define __WEBSOCKET_HANDLERS__

#include <FFat.h>
#include <PsychicHttp.h>
#include <PsychicWebSocket.h>

#include "playertask.h"

extern bool _paused;
extern playList_t playList;
extern void playListEnd();
extern const char *FAVORITES_FOLDER;

#endif