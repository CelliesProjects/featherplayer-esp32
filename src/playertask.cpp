
#include "playertask.h"
#include "playList.h"

#define VS1053_CS 6
#define VS1053_DCS 10
#define VS1053_DREQ 9
#define SDREADER_CS 5

extern playList_t playList;
extern uint8_t _playerVolume;
extern SemaphoreHandle_t spiMutex;
extern QueueHandle_t playerQueue;
extern bool _paused;
extern size_t _savedPosition;

void updateTFT()
{
    /*tftMessage msg;
    playListItem item;

    msg.action = tftMessage::CLEAR_SCREEN;
    xQueueSend(tftQueue, &msg, portMAX_DELAY);

    msg.action = tftMessage::SHOW_STATION;
    snprintf(msg.str, sizeof(msg.str), "%s", playList.name(playList.currentItem()).c_str());
    xQueueSend(tftQueue, &msg, portMAX_DELAY);*/
}

void playerTask(void *parameter)
{
    static ESP32_VS1053_Stream audio;

    xSemaphoreTake(spiMutex, portMAX_DELAY);
    if (!audio.startDecoder(VS1053_CS, VS1053_DCS, VS1053_DREQ) || !audio.isChipConnected())
    {
        log_e("VS1053 board could not init. System HALTED!");
        while (1)
            delay(100);
    }
    xSemaphoreGive(spiMutex);

    //playListEnd();

    log_i("Ready to rock!");

    while (1)
    {
        static playerMessage msg{};
        if (xQueueReceive(playerQueue, &msg, pdMS_TO_TICKS(25)) == pdTRUE)
        {
            switch (msg.action)
            {

            case playerMessage::SET_VOLUME:
                _playerVolume = msg.value > VS1053_MAXVOLUME ? VS1053_MAXVOLUME : msg.value;
                //ws.printfAll("%s\n%i\n", VOLUME_HEADER, _playerVolume);
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                audio.setVolume(_playerVolume);
                xSemaphoreGive(spiMutex);
                break;

            case playerMessage::START_ITEM:
                if (msg.value >= playList.size())
                {
                    playListEnd();
                    break;
                }

                playList.setCurrentItem(msg.value);
                if (audio.isRunning())
                {
                    xSemaphoreTake(spiMutex, portMAX_DELAY);
                    audio.stopSong();
                    xSemaphoreGive(spiMutex);
                }

                if (_paused)
                    //ws.textAll("status\nplaying\n");

                if (!_paused && !msg.offset)
                {
                    /*tftMessage msg;
                    msg.action = tftMessage::CLEAR_SCREEN;
                    xQueueSend(tftQueue, &msg, portMAX_DELAY);
                    snprintf(msg.str, sizeof(msg.str), "connecting...");
                    msg.action = tftMessage::SYSTEM_MESSAGE;
                    xQueueSend(tftQueue, &msg, portMAX_DELAY);*/
                }

                /* keep trying until some stream starts or we reach the end of playlist */
                while (playList.currentItem() < playList.size())
                {
                    if (!_paused && !msg.offset)
                    {
                        //ws.textAll("streamtitle\n\n");
                        char buff[255];
                        snprintf(buff, sizeof(buff), "showstation\n%s\n%s\n",
                                 playList.name(playList.currentItem()).c_str(),
                                 typeStr[playList.type(playList.currentItem())]);
                        //ws.textAll(buff);
                    }

                    if (!msg.offset)
                        //ws.textAll(currentPlayingItem());

                    xSemaphoreTake(spiMutex, portMAX_DELAY);
                    const auto result = audio.connecttohost(playList.url(playList.currentItem()).c_str(), LIBRARY_USER, LIBRARY_PWD, msg.offset);
                    xSemaphoreGive(spiMutex);

                    if (result)
                        break;

                    log_w("item %i failed to start", playList.currentItem());
                    playList.setCurrentItem(playList.currentItem() + 1);
                }

                if (audio.isRunning() && !_paused && !msg.offset)
                    updateTFT();

                if (!audio.isRunning())
                    playListEnd();

                _paused = false;
                break;

            case playerMessage::PAUSE:
                _savedPosition = audio.position();
                _paused = true;
                [[fallthrough]];
            case playerMessage::STOPSONG:
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                audio.stopSong();
                xSemaphoreGive(spiMutex);
                break;
                /*
            case playerMessage::SETTONE:
                // implement it!
                break;
                */
            default:
                log_w("unhandled player message with number %i", msg.action);
            }
        }

        constexpr const auto MAX_UPDATE_FREQ_HZ = 3;
        constexpr const auto UPDATE_INTERVAL_MS = 1000 / MAX_UPDATE_FREQ_HZ;
        static unsigned long savedTime = millis();

        if (audio.size() && millis() - savedTime > UPDATE_INTERVAL_MS && audio.position() != _savedPosition)
        {
            log_d("Buffer status: %s", audio.bufferStatus());

            //ws.printfAll("progress\n%i\n%i\n", audio.position(), audio.size());
            savedTime = millis();
            _savedPosition = audio.position();
        }

        if (audio.isRunning())
        {
            xSemaphoreTake(spiMutex, portMAX_DELAY);
            audio.loop();
            xSemaphoreGive(spiMutex);
        }

    }
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

    log_i("playlist ended");
}