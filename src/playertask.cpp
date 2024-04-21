
#include "playertask.h"

void updateTFT()
{
    /*
    tftMessage msg;
    playListItem item;

    msg.action = tftMessage::CLEAR_SCREEN;
    xQueueSend(tftQueue, &msg, portMAX_DELAY);

    msg.action = tftMessage::SHOW_STATION;
    snprintf(msg.str, sizeof(msg.str), "%s", playList.name(playList.currentItem()).c_str());
    xQueueSend(tftQueue, &msg, portMAX_DELAY);
    */
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

    playListEnd();

    log_i("Ready to rock!");

    while (1)
    {
        static playerMessage msg;
        if (xQueueReceive(playerQueue, &msg, pdMS_TO_TICKS(25)) == pdTRUE)
        {
            switch (msg.action)
            {

            case playerMessage::SET_VOLUME:
                _playerVolume = msg.value > VS1053_MAXVOLUME ? VS1053_MAXVOLUME : msg.value;
                // ws clients updaten!
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                audio.setVolume(_playerVolume);
                xSemaphoreGive(spiMutex);
                break;

            case playerMessage::START_ITEM:
                if (playList.size() < msg.value)
                {
                    log_e("out of bound item requested");
                    break;
                }

                playList.setCurrentItem(msg.value);

                if (audio.isRunning())
                {
                    xSemaphoreTake(spiMutex, portMAX_DELAY);
                    audio.stopSong();
                    xSemaphoreGive(spiMutex);
                }

                {
                    serverMessage msg;
                    msg.str[0] = 0;
                    msg.type = serverMessage::WS_UPDATE_STREAMTITLE;
                    xQueueSend(serverQueue, &msg, portMAX_DELAY);
                }

                /* keep trying until some stream starts or we reach the end of playlist */
                while (playList.currentItem() < playList.size())
                {
                    if (!_paused && !msg.offset)
                    {
                        {
                            tftMessage msg;
                            msg.action = tftMessage::CLEAR_SCREEN;
                            xQueueSend(tftQueue, &msg, portMAX_DELAY);

                            msg.action = tftMessage::SHOW_STATION;
                            snprintf(msg.str, sizeof(msg.str), "%s", playList.name(playList.currentItem()).c_str());
                            xQueueSend(tftQueue, &msg, portMAX_DELAY);

                            msg.action = tftMessage::SHOW_TITLE; // send empty title to erase the scrollbar
                            msg.str[0] = 0;
                            xQueueSend(tftQueue, &msg, portMAX_DELAY);
                        }
                        serverMessage msg;
                        msg.type = serverMessage::WS_UPDATE_NOWPLAYING;
                        xQueueSend(serverQueue, &msg, portMAX_DELAY);

                        msg.type = serverMessage::WS_UPDATE_STATION;
                        snprintf(msg.str, sizeof(msg.str), "%s", playList.name(playList.currentItem()).c_str());
                        xQueueSend(serverQueue, &msg, portMAX_DELAY);
                    }

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
            {
                serverMessage msg;
                msg.type = serverMessage::WS_UPDATE_PROGRESS;
                msg.value = audio.position();
                msg.value2 = audio.size();
                xQueueSend(serverQueue, &msg, portMAX_DELAY);
            }
            tftMessage msg;
            msg.action = tftMessage::PROGRESS_BAR;
            msg.value1 = audio.position();
            msg.value2 = audio.size();
            xQueueSend(tftQueue, &msg, portMAX_DELAY);

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

    {
        serverMessage msg;
        msg.type = serverMessage::WS_UPDATE_NOWPLAYING;
        xQueueSend(serverQueue, &msg, portMAX_DELAY);

        snprintf(msg.str, sizeof(msg.str), "%s %s", PROGRAM_NAME, GIT_VERSION);
        msg.type = serverMessage::WS_UPDATE_STATION;
        xQueueSend(serverQueue, &msg, portMAX_DELAY);

        snprintf(msg.str, sizeof(msg.str), "%s", "Search API provided by: <a href=\"https://www.radio-browser.info/\" target=\"_blank\"><span style=\"white-space:nowrap;\">radio-browser.info</span></a>");
        msg.type = serverMessage::WS_UPDATE_STREAMTITLE;
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
    }

    tftMessage msg;
    msg.action = tftMessage::CLEAR_SCREEN;
    xQueueSend(tftQueue, &msg, portMAX_DELAY);

    msg.action = tftMessage::SHOW_IPADDRESS;
    xQueueSend(tftQueue, &msg, portMAX_DELAY);

    msg.action = tftMessage::SHOW_TITLE;
    snprintf(msg.str, sizeof(msg.str), "%s - %s", PROGRAM_NAME, GIT_VERSION);
    xQueueSend(tftQueue, &msg, portMAX_DELAY);
}