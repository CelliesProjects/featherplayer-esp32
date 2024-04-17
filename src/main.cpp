#include <Arduino.h>
#include <WiFi.h>
//#include <FS.h>
//#include <SD.h>

#include "tfttask.h"
#include "playertask.h"
#include "servertask.h"

playList_t playList;

const char PROGRAM_NAME[] = {"featherplayer-esp32"};
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


