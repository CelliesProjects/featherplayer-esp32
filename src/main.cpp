#include <Arduino.h>
#include <WiFi.h>
//#include <FS.h>
//#include <SD.h>

#include "tfttask.h"
#include "playertask.h"
#include "servertask.h"

playList_t playList;

const char *PROGRAM_NAME[] = {"featherplayer-esp32"};
static uint8_t _playerVolume = VS1053_INITIALVOLUME;
static size_t _savedPosition = 0;
static bool _paused = false;

SemaphoreHandle_t spiMutex = nullptr; // SPI bus is shared between playertask -VS1053- and tfttask -ST7889-

QueueHandle_t tftQueue = nullptr;
QueueHandle_t playerQueue = nullptr;
QueueHandle_t serverQueue = nullptr;

void setup()
{
    // put your setup code here, to run once:
}

void loop()
{
    // put your main code here, to run repeatedly:
}

void playListEnd()
{
    playList.setCurrentItem(PLAYLIST_STOPPED);

    // to limit the scope of the different msgs we use extra scope blocks here
    {
        /*playerMessage msg;
        // websocket: send current item
        msg.action = playerMessage::WS_UPDATE_NOWPLAYING;
        xQueueSend(playerQueue, &msg, portMAX_DELAY);

        // websocket: send program name and version
        snprintf(msg.str, sizeof(msg.str), "%s %s", PROGRAM_NAME, GIT_VERSION);
        msg.action = playerMessage::WS_UPDATE_STATION;
        xQueueSend(playerQueue, &msg, portMAX_DELAY);

        // websocket: send radiobrowser link as streamtitle
        snprintf(msg.str, sizeof(msg.str), "%s", "Search API provided by: <a href=\"https://www.radio-browser.info/\" target=\"_blank\"><span style=\"white-space:nowrap;\">radio-browser.info</span></a>");
        msg.action = playerMessage::WS_UPDATE_STREAMTITLE;
        xQueueSend(playerQueue, &msg, portMAX_DELAY);*/
    }

    // clear tft and show system info
    tftMessage msg;
    msg.action = tftMessage::CLEAR_SCREEN;
    xQueueSend(tftQueue, &msg, portMAX_DELAY);

    msg.action = tftMessage::SHOW_IPADDRESS;
    xQueueSend(tftQueue, &msg, portMAX_DELAY);

    msg.action = tftMessage::SHOW_TITLE;
    snprintf(msg.str, sizeof(msg.str), "%s - %s", (char *)PROGRAM_NAME, (char *)GIT_VERSION);
    xQueueSend(tftQueue, &msg, portMAX_DELAY);

    log_d("playlist ended");
}
