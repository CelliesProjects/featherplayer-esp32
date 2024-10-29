#include "playertask.h"

void playListEnd()
{
    playList.setCurrentItem(PLAYLIST_STOPPED);

    sendTftMessage(tftMessage::SHOW_IPADDRESS);
    sendTftMessage(tftMessage::SHOW_CLOCK);

    char buff[150];
    snprintf(buff, 150, "%s %s", PROGRAM_NAME, GIT_VERSION);
    sendTftMessage(tftMessage::SHOW_TITLE, buff);
    sendServerMessage(serverMessage::WS_UPDATE_STATION, buff);

    snprintf(buff, 150, "%s", "Search API provided by: <a href=\"https://www.radio-browser.info/\" target=\"_blank\"><span style=\"white-space:nowrap;\">radio-browser.info</span></a>");
    sendServerMessage(serverMessage::WS_UPDATE_STREAMTITLE, buff);
    sendServerMessage(serverMessage::WS_UPDATE_NOWPLAYING);
}

static void startItem(ESP32_VS1053_Stream &audio, playerMessage &msg)
{
    if (audio.isRunning())
    {
        xSemaphoreTake(spiMutex, portMAX_DELAY);
        audio.stopSong();
        xSemaphoreGive(spiMutex);
    }

    if (msg.value >= playList.size())
    {
        log_w("playlist ended");
        playListEnd();
        return;
    }

    playList.setCurrentItem(msg.value);

    if (_paused)
        sendServerMessage(serverMessage::WS_UPDATE_STATUS, "playing");
    _paused = false;

    /* keep trying until some stream starts or we reach the end of playlist */
    while (playList.currentItem() < playList.size())
    {
        if (!msg.offset)
        {
            sendTftMessage(tftMessage::SHOW_TITLE, "\0");
            sendTftMessage(tftMessage::CLEAR_SCREEN);
            sendTftMessage(tftMessage::SHOW_STATION, playList.name(playList.currentItem()).c_str());
            sendServerMessage(serverMessage::WS_UPDATE_NOWPLAYING);
            sendServerMessage(serverMessage::WS_UPDATE_STREAMTITLE);
            sendServerMessage(serverMessage::WS_UPDATE_STATION, playList.name(playList.currentItem()).c_str());
        }

        xSemaphoreTake(spiMutex, portMAX_DELAY);
        const auto success = audio.connecttohost(playList.url(playList.currentItem()).c_str(), LIBRARY_USER, LIBRARY_PWD, msg.offset);
        xSemaphoreGive(spiMutex);

        if (success)
            break;

        log_w("item %i failed to start", playList.currentItem());

        msg.offset = 0; // do not loop through the playlist with an offset
        playList.setCurrentItem(playList.currentItem() + 1);

        sendTftMessage(tftMessage::CLEAR_SCREEN);
    }

    if (audio.isRunning())
    {
        char buff[32];
        if (audio.bitrate())
            snprintf(buff, 32, "%s %u kbps", audio.currentCodec(), audio.bitrate());
        else
            snprintf(buff, 32, "%s", audio.currentCodec());
        sendTftMessage(tftMessage::SHOW_CODEC, buff);
    }
    else
        playListEnd();
}

void playerTask(void *parameter)
{
    sendTftMessage(tftMessage::SYSTEM_MESSAGE, "Starting codec...");

    static ESP32_VS1053_Stream audio;

    xSemaphoreTake(spiMutex, portMAX_DELAY);
    if (!audio.startDecoder(VS1053_CS, VS1053_DCS, VS1053_DREQ) || !audio.isChipConnected())
    {
        log_e("VS1053 board could not init. System HALTED!");
        sendTftMessage(tftMessage::SYSTEM_MESSAGE, "VS1053 ERROR. System HALTED.");
        while (1)
            delay(100);
    }
    xSemaphoreGive(spiMutex);

    playListEnd(); // this puts the audio system in a known state

    log_i("Ready to rock!");

    while (1)
    {
        static playerMessage msg;
        if (xQueueReceive(playerQueue, &msg, pdMS_TO_TICKS(25)) == pdTRUE)
        {
            switch (msg.type)
            {

            case playerMessage::SET_VOLUME:
                _playerVolume = msg.value > VS1053_MAXVOLUME ? VS1053_MAXVOLUME : msg.value;
                sendServerMessage(serverMessage::WS_UPDATE_VOLUME, NULL, false, _playerVolume);

                xSemaphoreTake(spiMutex, portMAX_DELAY);
                audio.setVolume(_playerVolume);
                xSemaphoreGive(spiMutex);
                break;

            case playerMessage::START_ITEM:
                startItem(audio, msg);
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
                log_w("unhandled player message with number %i", msg.type);
            }
        }

        constexpr const auto MAX_UPDATE_FREQ_HZ = 3;
        constexpr const auto UPDATE_INTERVAL_MS = 1000 / MAX_UPDATE_FREQ_HZ;
        static unsigned long savedTime = millis();

        if (audio.size() && millis() - savedTime > UPDATE_INTERVAL_MS)
        {
            log_d("Buffer status: %s", audio.bufferStatus());
            sendTftMessage(tftMessage::PROGRESS_BAR, NULL, audio.position(), audio.size());
            sendServerMessage(serverMessage::WS_UPDATE_PROGRESS, NULL, false, audio.position(), audio.size());
            savedTime = millis();
        }

        if (audio.isRunning())
        {
            size_t used, capacity;
            audio.bufferStatus(used, capacity);
            sendTftMessage(tftMessage::BUFFER_STATUS, NULL, used, capacity);

            xSemaphoreTake(spiMutex, portMAX_DELAY);
            audio.loop();
            xSemaphoreGive(spiMutex);
        }
    }
}
