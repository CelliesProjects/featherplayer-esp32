#include "playertask.h"

void playerTask(void *parameter)
{
    {
        tftMessage msg;
        msg.action = tftMessage::SYSTEM_MESSAGE;
        snprintf(msg.str, sizeof(msg.str), "Starting codec...");
        xQueueSend(tftQueue, &msg, portMAX_DELAY);
        taskYIELD();
    }
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

    [[maybe_unused]] static size_t _savedPosition = 0;

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
                {
                    serverMessage msg;
                    msg.type = serverMessage::WS_UPDATE_VOLUME;
                    msg.value = _playerVolume;
                    xQueueSend(serverQueue, &msg, portMAX_DELAY);
                }
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                audio.setVolume(_playerVolume);
                xSemaphoreGive(spiMutex);
                break;

            case playerMessage::START_ITEM:
                if (audio.isRunning())
                {
                    xSemaphoreTake(spiMutex, portMAX_DELAY);
                    audio.stopSong();
                    xSemaphoreGive(spiMutex);
                }

                if (msg.value >= playList.size())
                {
                    log_w("playback stops because requested item is out of bounds");
                    _paused = false;
                    playListEnd();
                    break;
                }

                playList.setCurrentItem(msg.value);

                {
                    serverMessage msg;
                    msg.str[0] = 0;
                    msg.type = serverMessage::WS_UPDATE_STREAMTITLE; // send empty title to websocket clients
                    xQueueSend(serverQueue, &msg, portMAX_DELAY);
                }

                {
                    tftMessage msg;
                    msg.str[0] = 0;
                    msg.action = tftMessage::SHOW_TITLE; // send empty title to tft to erase the scrollbar
                    xQueueSend(tftQueue, &msg, portMAX_DELAY);
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
                        }
                        taskYIELD();
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
                {
                    tftMessage msg;
                    msg.action = tftMessage::SHOW_CODEC;
                    if (audio.bitrate())
                        snprintf(msg.str, sizeof(msg.str), "%s %u kbps", audio.currentCodec(), audio.bitrate());
                    else
                        snprintf(msg.str, sizeof(msg.str), "%s", audio.currentCodec());
                    xQueueSend(tftQueue, &msg, portMAX_DELAY);
                    taskYIELD();
                }

                _paused = false;

                if (!audio.isRunning())
                    playListEnd();
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

        if (audio.isRunning()) /* send buffer status to tft */
        {
            tftMessage msg;
            audio.bufferStatus(msg.value1, msg.value2);
            msg.action = tftMessage::BUFFER_STATUS;
            xQueueSend(tftQueue, &msg, portMAX_DELAY);
        }

        if (audio.size() && millis() - savedTime > UPDATE_INTERVAL_MS)
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