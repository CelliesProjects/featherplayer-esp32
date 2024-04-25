#include <Arduino.h>
#include <WiFi.h>
// #include <FS.h>
// #include <SD.h>

#include "websocketEventHandler.h"
#include "icons.h"
#include "percentEncode.h"
#include "WiFiCredentials.h"

const char *PROGRAM_NAME = "featherplayer-esp32";
const char *FAVORITES_FOLDER = "/"; /* if this is a folder use a closing slash */
playList_t playList;
uint8_t _playerVolume = VS1053_INITIALVOLUME;
bool _paused = false;

SemaphoreHandle_t spiMutex = nullptr; // SPI bus is shared between playertask -VS1053- and tfttask -ST7789-

QueueHandle_t tftQueue = nullptr;
QueueHandle_t playerQueue = nullptr;
QueueHandle_t serverQueue = nullptr;

void setup()
{
    [[maybe_unused]] const uint32_t idf = ESP_IDF_VERSION_PATCH + ESP_IDF_VERSION_MINOR * 10 + ESP_IDF_VERSION_MAJOR * 100;
    [[maybe_unused]] const uint32_t ard = ESP_ARDUINO_VERSION_PATCH + ESP_ARDUINO_VERSION_MINOR * 10 + ESP_ARDUINO_VERSION_MAJOR * 100;
    log_i("ESP32 IDF Version %d.%d.%d", idf / 100 % 10, idf / 10 % 10, idf % 10);
    log_i("ESP32 Arduino Version %d.%d.%d", ard / 100 % 10, ard / 10 % 10, ard % 10);
    log_i("CPU: %iMhz", getCpuFrequencyMhz());
    log_i("Found %i presets", NUMBER_OF_PRESETS);

    SPI.setHwCs(true);
    SPI.begin(SCK, MISO, MOSI);

    // mountSDcard(); /* !!! without mutex! */

    spiMutex = xSemaphoreCreateMutex();
    if (!spiMutex)
    {
        log_e("FATAL error! could not create SPI mutex. System HALTED!");
        while (1)
            delay(100);
    }

    tftQueue = xQueueCreate(2, sizeof(struct tftMessage));
    if (!tftQueue)
    {
        log_e("FATAL error! could not create display queue. System HALTED!");
        while (1)
            delay(100);
    }

    playerQueue = xQueueCreate(2, sizeof(struct playerMessage));
    if (!playerQueue)
    {
        log_e("FATAL error! could not create player queue. System HALTED!");
        while (1)
            delay(100);
    }

    serverQueue = xQueueCreate(5, sizeof(struct serverMessage));
    if (!serverQueue)
    {
        log_e("FATAL error! could not create server queue. System HALTED!");
        while (1)
            delay(100);
    }

    BaseType_t taskResult = xTaskCreate(
        tftTask,
        "tftTask",
        2048,
        NULL,
        (tskIDLE_PRIORITY + 11) | portPRIVILEGE_BIT,
        NULL);

    if (taskResult != pdPASS)
    {
        log_e("FATAL error! Could not create tftTask. System HALTED!");
        while (1)
            delay(100);
    }

    /* check if a ffat partition is defined and halt the system if it is not defined*/
    if (!esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "ffat"))
    {
        log_e("FATAL ERROR! No FFat partition defined. System is halted.\nCheck 'Tools>Partition Scheme' in the Arduino IDE and select a partition table with a FFat partition.");
        while (true)
            delay(1000); /* system is halted */
    }

    /* partition is defined - try to mount it */
    if (FFat.begin(0, "", 2)) // see: https://github.com/lorol/arduino-esp32fs-plugin#notes-for-fatfs
        log_i("FFat mounted");

    /* partition is present, but does not mount so now we just format it */
    else
    {
        tftMessage msg;
        msg.action = tftMessage::SYSTEM_MESSAGE;
        snprintf(msg.str, sizeof(msg.str), "Formatting, please wait...");
        xQueueSend(tftQueue, &msg, portMAX_DELAY);
        delay(2);

        log_i("Formatting FFat...");
        if (!FFat.format(true, (char *)"ffat") || !FFat.begin(0, "", 2))
        {
            log_e("FFat error while formatting. Halting.");
            while (true)
                delay(1000); /* system is halted */
        }
    }

    {
        tftMessage msg;
        msg.action = tftMessage::SYSTEM_MESSAGE;
        snprintf(msg.str, sizeof(msg.str), "Connecting WiFi...");
        xQueueSend(tftQueue, &msg, portMAX_DELAY);
    }
    delay(2);

    log_i("FeatherPlayer esp32 connecting to %s", SSID);

    WiFi.useStaticBuffers(true);
    WiFi.begin(SSID, PSK);
    WiFi.setSleep(false);
    while (!WiFi.isConnected())
        delay(10);

    log_i("WiFi connected - IP %s", WiFi.localIP().toString().c_str());

    configTzTime(TIMEZONE, NTP_POOL);

    struct tm timeinfo
    {
    };

    log_i("Waiting for NTP sync...");
    {
        tftMessage msg;
        msg.action = tftMessage::SYSTEM_MESSAGE;
        snprintf(msg.str, sizeof(msg.str), "Synching NTP...");
        xQueueSend(tftQueue, &msg, portMAX_DELAY);
    }
    
    delay(2);

    while (!getLocalTime(&timeinfo, 0))
        delay(10);

    log_i("Synced");

    taskResult = xTaskCreate(
        playerTask,
        "playerTask",
        6144,
        NULL,
        (tskIDLE_PRIORITY + 10),
        NULL);

    if (taskResult != pdPASS)
    {
        log_e("FATAL error! Could not create playerTask. System HALTED!");
        while (1)
            delay(100);
    }

    taskResult = xTaskCreate(
        serverTask,
        "serverTask",
        4000,
        NULL,
        tskIDLE_PRIORITY + 8,
        NULL);

    if (taskResult != pdPASS)
    {
        log_e("FATAL error! Could not create serverTask. System HALTED!");
        while (1)
            delay(100);
    }

    vTaskDelete(NULL);
}

void loop() {}

void audio_eof_stream(const char *info)
{
    playerMessage msg;
    msg.action = playerMessage::START_ITEM;
    msg.value = playList.currentItem() + 1;
    xQueueSend(playerQueue, &msg, portMAX_DELAY);
}

void audio_showstreamtitle(const char *info)
{
    log_d("STREAMTITLE: %s", info);
    {
        serverMessage msg;
        msg.type = serverMessage::WS_UPDATE_STREAMTITLE;
        snprintf(msg.str, sizeof(msg.str), "%s", info);
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
    }

    tftMessage msg;
    msg.action = tftMessage::SHOW_TITLE;
    snprintf(msg.str, sizeof(msg.str), "%s", info);
    xQueueSend(tftQueue, &msg, portMAX_DELAY);
}

void audio_showstation(const char *info)
{
    log_d("STATION: %s", info);

    serverMessage msg;
    msg.type = serverMessage::WS_UPDATE_STATION;
    snprintf(msg.str, sizeof(msg.str), "%s", info);
    xQueueSend(serverQueue, &msg, portMAX_DELAY);    
}
