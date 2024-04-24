
#include "websocketEventHandler.h"

void websocketEventHandler(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
    case WS_EVT_CONNECT:
    {
        log_d("client %i connected on %s", client->id(), server->url());
        serverMessage msg;
        msg.singleClient = true;
        msg.value = client->id();
        msg.type = serverMessage::WS_UPDATE_PLAYLIST;
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
        msg.type = serverMessage::WS_UPDATE_NOWPLAYING;
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
        msg.type = serverMessage::WS_UPDATE_FAVORITES;
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
        msg.type = serverMessage::WS_UPDATE_VOLUME;
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
        msg.type = serverMessage::WS_UPDATE_STREAMTITLE;
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
        msg.type = serverMessage::WS_UPDATE_STATION;
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
        break;
    }
    case WS_EVT_DISCONNECT:
        log_d("client %i disconnected from %s", client->id(), server->url());
        break;
    case WS_EVT_ERROR:
        log_e("ws error");
        break;
    case WS_EVT_PONG:
        log_d("ws pong");
        break;
    case WS_EVT_DATA:
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->opcode == WS_TEXT)
        {
            if (info->final && info->index == 0 && info->len == len)
                handleSingleFrame(client, data, len);
            else
                handleMultiFrame(client, data, len, info);
        }
        break;
    }
    default:
        log_w("unhandled ws event!");
    }

    log_d("Heap: %d Free: ", ESP.getHeapSize(), ESP.getFreeHeap());
    log_d("Smallest free stack: %i bytes", uxTaskGetStackHighWaterMark(NULL));
    log_d("Largest free continuous memory block: %i bytes", heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

static void updatePlaylistOverWebSocket()
{
    serverMessage msg;
    msg.type = serverMessage::WS_UPDATE_PLAYLIST;
    xQueueSend(serverQueue, &msg, portMAX_DELAY);
}

static void handleFavoriteToPlaylist(AsyncWebSocketClient *client, const char *filename, const bool startNow)
{
    if (PLAYLIST_MAX_ITEMS == playList.size())
    {
        log_e("ERROR! Could not add %s to playlist", filename);
        serverMessage msg;
        msg.type = serverMessage::WS_PASS_MESSAGE;
        msg.singleClient = true;
        msg.value = client->id();
        snprintf(msg.str, sizeof(msg.str), "ERROR: Could not add '%s' to playlist", filename);
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
        return;
    }
    char path[strlen(FAVORITES_FOLDER) + strlen(filename) + 1];
    snprintf(path, sizeof(path), "%s%s", FAVORITES_FOLDER, filename);
    File file = FFat.open(path);
    if (!file)
    {
        log_e("ERROR! Could not open %s", filename);
        serverMessage msg;
        msg.type = serverMessage::WS_PASS_MESSAGE;
        msg.singleClient = true;
        msg.value = client->id();
        snprintf(msg.str, sizeof(msg.str), "ERROR: Could not add '%s' to playlist", filename);
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
        return;
    }
    char url[file.size() + 1];
    auto cnt = 0;
    char ch = (char)file.read();
    while (ch != '\n' && file.available())
    {
        url[cnt++] = ch;
        ch = (char)file.read();
    }
    url[cnt] = 0;
    file.close();
    const auto previousSize = playList.size();
    playList.add({HTTP_FAVORITE, filename, url, 0});

    log_d("favorite to playlist: %s -> %s", filename, url);

    if (startNow || playList.currentItem() == PLAYLIST_STOPPED)
    {
        playList.setCurrentItem(previousSize);
        playerMessage msg;
        msg.action = playerMessage::START_ITEM;
        msg.value = playList.currentItem();
        xQueueSend(playerQueue, &msg, portMAX_DELAY);
    }
}

static bool saveItemToFavorites(AsyncWebSocketClient *client, const char *filename, const playListItem &item)
{
    if (!strlen(filename))
    {
        log_e("ERROR! no filename");
        return false;
    }
    switch (item.type)
    {
    case HTTP_FILE:
        log_d("file (wont save)%s", item.url.c_str());
        return false;
    case HTTP_PRESET:
        log_d("preset (wont save) %s %s", preset[item.index].name.c_str(), preset[item.index].url.c_str());
        return false;
    case HTTP_FOUND:
    case HTTP_FAVORITE:
    {
        char path[strlen(FAVORITES_FOLDER) + strlen(filename) + 1];
        snprintf(path, sizeof(path), "%s%s", FAVORITES_FOLDER, filename);
        File file = FFat.open(path, FILE_WRITE);
        if (!file)
        {
            log_e("failed to open '%s' for writing", filename);
            serverMessage msg;
            msg.singleClient = true;
            msg.value = client->id();
            snprintf(msg.str, sizeof(msg.str), "ERROR: Could not open '%s' for writing!", filename);
            xQueueSend(serverQueue, &msg, portMAX_DELAY);
            return false;
        }
        char url[item.url.length() + 2];
        snprintf(url, sizeof(url), "%s\n", item.url.c_str());
        const auto bytesWritten = file.print(url);
        file.close();
        if (bytesWritten < strlen(url))
        {
            log_e("ERROR! Saving '%s' failed - disk full?", filename);
            serverMessage msg;
            msg.type = serverMessage::WS_PASS_MESSAGE;
            snprintf(msg.str, sizeof(msg.str), "ERROR: Could not completely save '%s' to favorites!", filename);
            xQueueSend(serverQueue, &msg, portMAX_DELAY);
            return false;
        }
        serverMessage msg;
        msg.type = serverMessage::WS_PASS_MESSAGE;
        snprintf(msg.str, sizeof(msg.str), "Saved '%s' to favorites!", filename);
        xQueueSend(serverQueue, &msg, portMAX_DELAY);
        return true;
    }
    break;
    default:
    {
        log_w("Unhandled item.type.");
        return false;
    }
    }
}

void handleSingleFrame(AsyncWebSocketClient *client, uint8_t *data, size_t len)
{
    data[len] = 0;
    char *pch = strtok(reinterpret_cast<char *>(data), "\n");
    if (!pch)
        return;

    if (_paused && !strcmp("unpause", pch))
    {
        /*playerMessage msg;
        msg.action = playerMessage::START_ITEM;
        msg.offset = _savedPosition;
        msg.value = playList.currentItem();
        xQueueSend(playerQueue, &msg, portMAX_DELAY);
        return;*/
    }

    if (!_paused && !strcmp("pause", pch))
    {
        /*
        playerMessage msg;
        msg.action = playerMessage::PAUSE;
        xQueueSend(playerQueue, &msg, portMAX_DELAY);

        msg.action = playerMessage::WS_PASS_MESSAGE;
        snprintf(msg.str, sizeof(msg.str), "status\npaused\n");
        xQueueSend(playerQueue, &msg, portMAX_DELAY);
        */
    }

    else if (!strcmp("volume", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return;
        const uint8_t volume = atoi(pch);
        playerMessage msg;
        msg.action = playerMessage::SET_VOLUME;
        msg.value = volume > VS1053_MAXVOLUME ? VS1053_MAXVOLUME : volume;
        xQueueSend(playerQueue, &msg, portMAX_DELAY);
    }

    else if (!strcmp("previous", pch))
    {
        if (playList.currentItem() > 0)
        {
            playerMessage msg;
            msg.action = playerMessage::START_ITEM;
            msg.value = playList.currentItem() - 1;
            xQueueSend(playerQueue, &msg, portMAX_DELAY);
        }
    }

    else if (!strcmp("next", pch))
    {
        if (playList.currentItem() == PLAYLIST_STOPPED)
            return;
        if (playList.currentItem() < playList.size() - 1)
        {
            playerMessage msg;
            msg.action = playerMessage::START_ITEM;
            msg.value = playList.currentItem() + 1;
            xQueueSend(playerQueue, &msg, portMAX_DELAY);
        }
    }

    else if (!strcmp("filetoplaylist", pch) || !strcmp("_filetoplaylist", pch))
    {
        const bool startnow = (pch[0] == '_');
        const uint32_t previousSize = playList.size();
        pch = strtok(NULL, "\n");
        while (pch)
        {
            playList.add({HTTP_FILE, "", pch, 0});
            pch = strtok(NULL, "\n");
        }
        const uint32_t itemsAdded{playList.size() - previousSize};
        log_d("Added %i library items to playlist", itemsAdded);

        if (!itemsAdded)
            return;

        if (itemsAdded > 1)
        {
            serverMessage msg;
            msg.type = serverMessage::WS_PASS_MESSAGE;
            msg.singleClient = true;
            msg.value = client->id();
            snprintf(msg.str, sizeof(msg.str), "Added %u items to playlist", itemsAdded);
            xQueueSend(serverQueue, &msg, portMAX_DELAY);
        }

        updatePlaylistOverWebSocket();

        if (startnow || playList.currentItem() == PLAYLIST_STOPPED)
        {
            playerMessage msg;
            msg.action = playerMessage::START_ITEM;
            msg.value = previousSize;
            xQueueSend(playerQueue, &msg, portMAX_DELAY);
        }
    }

    else if (!strcmp("playitem", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return;
        const uint8_t index = atoi(pch);
        if (index < playList.size())
        {
            playerMessage msg;
            msg.action = playerMessage::START_ITEM;
            msg.value = index;
            xQueueSend(playerQueue, &msg, portMAX_DELAY);
        }
    }

    else if (!strcmp("deleteitem", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return;
        const uint8_t index = atoi(pch);
        if (index >= playList.size())
            return;

        playList.remove(index);
        // deleted item was before current item
        if (index < playList.currentItem())
        {
            playList.setCurrentItem(playList.currentItem() - 1);
            updatePlaylistOverWebSocket();
        }
        //  deleted item was the current item
        else if (playList.currentItem() == index)
        {
            // play the next item if there is one
            if (playList.currentItem() < playList.size())
            {
                playerMessage msg;
                msg.action = playerMessage::START_ITEM;
                msg.value = playList.currentItem();
                xQueueSend(playerQueue, &msg, portMAX_DELAY);
                updatePlaylistOverWebSocket();
            }
            else
            {
                playerMessage msg;
                msg.action = playerMessage::STOPSONG;
                xQueueSend(playerQueue, &msg, portMAX_DELAY);
                updatePlaylistOverWebSocket();
                playListEnd();
            }
        }

        // deleted item was after current item
        else
            updatePlaylistOverWebSocket();
    }

    else if (!strcmp("clearlist", pch))
    {
        if (!playList.size())
            return;
        playerMessage msg;
        msg.action = playerMessage::STOPSONG;
        xQueueSend(playerQueue, &msg, portMAX_DELAY);

        playList.clear();
        updatePlaylistOverWebSocket();
        playListEnd();
    }

    else if (!strcmp("presetstation", pch) || !strcmp("_presetstation", pch))
    {
        log_d("adding preset");
        const bool startnow = (pch[0] == '_');
        pch = strtok(NULL, "\n");
        if (!pch)
            return;
        const uint32_t index = atoi(pch);
        if (index >= NUMBER_OF_PRESETS)
            return;

        const uint32_t previousSize = playList.size();
        playList.add({HTTP_PRESET, "", "", index});
        if (playList.size() == previousSize)
        {
            serverMessage msg;
            msg.type = serverMessage::WS_PASS_MESSAGE;
            msg.singleClient = true;
            msg.value = client->id();
            snprintf(msg.str, sizeof(msg.str), "ERROR: Could not add '%s' to playlist", preset[index].name.c_str());
            xQueueSend(serverQueue, &msg, portMAX_DELAY);
            return;
        }

        log_d("Added '%s' to playlist", preset[index].name.c_str());

        updatePlaylistOverWebSocket();

        if (startnow || playList.currentItem() == PLAYLIST_STOPPED)
        {
            playerMessage msg;
            msg.action = playerMessage::START_ITEM;
            msg.value = playList.size() - 1;
            xQueueSend(playerQueue, &msg, portMAX_DELAY);
        }
    }

    else if (!strcmp("jumptopos", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return;
        playerMessage msg;
        msg.action = playerMessage::START_ITEM;
        msg.value = playList.currentItem();
        msg.offset = atoi(pch);
        xQueueSend(playerQueue, &msg, portMAX_DELAY);
    }

    else if (!strcmp("currenttofavorites", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return;
        playListItem item;
        playList.get(playList.currentItem(), item);
        if (saveItemToFavorites(client, pch, item))
        {
            serverMessage msg;
            msg.type = serverMessage::WS_UPDATE_FAVORITES;
            xQueueSend(serverQueue, &msg, portMAX_DELAY);
        }
        else
        {
            serverMessage msg;
            msg.type = serverMessage::WS_PASS_MESSAGE;
            snprintf(msg.str, sizeof(msg.str), "ERROR: Could not add '%s' to playlist!", pch);
            xQueueSend(serverQueue, &msg, portMAX_DELAY);
        }
    }

    else if (!strcmp("favoritetoplaylist", pch) || !strcmp("_favoritetoplaylist", pch))
    {
        const bool startNow = (pch[0] == '_');
        pch = strtok(NULL, "\n");
        if (!pch)
            return;

        if (playList.size() == PLAYLIST_MAX_ITEMS)
        {
            serverMessage msg;
            msg.type = serverMessage::WS_PASS_MESSAGE;
            msg.singleClient = true;
            msg.value = client->id();
            snprintf(msg.str, sizeof(msg.str), "ERROR: Could not add '%s' to playlist!", pch);
            xQueueSend(serverQueue, &msg, portMAX_DELAY);
            return;
        }
        const auto cnt = playList.size();
        handleFavoriteToPlaylist(client, pch, startNow);
        if (playList.size() > cnt)
            updatePlaylistOverWebSocket();
    }

    else if (!strcmp("deletefavorite", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return;
        char filename[strlen(FAVORITES_FOLDER) + strlen(pch) + 1];
        snprintf(filename, sizeof(filename), "%s%s", FAVORITES_FOLDER, pch);
        if (!FFat.remove(filename))
        {
            serverMessage msg;
            msg.type = serverMessage::WS_PASS_MESSAGE;
            msg.singleClient = true;
            msg.value = client->id();
            snprintf(msg.str, sizeof(msg.str), "ERROR: Could not delete %s", pch);
            xQueueSend(serverQueue, &msg, portMAX_DELAY);
        }
        else
        {
            serverMessage msg;
            msg.type = serverMessage::WS_UPDATE_FAVORITES;
            xQueueSend(serverQueue, &msg, portMAX_DELAY);
        }
    }

    else if (!strcmp("foundlink", pch) || !strcmp("_foundlink", pch))
    {
        if (playList.size() == PLAYLIST_MAX_ITEMS)
        {
            serverMessage msg;
            msg.type = serverMessage::WS_PASS_MESSAGE;
            msg.singleClient = true;
            msg.value = client->id();
            snprintf(msg.str, sizeof(msg.str), "ERROR: Could not add new url to playlist");
            xQueueSend(serverQueue, &msg, portMAX_DELAY);
            return;
        }
        const char *url = strtok(NULL, "\n");
        if (!url)
            return;
        const char *name = strtok(NULL, "\n");
        if (!name)
            return;

        playList.add({HTTP_FOUND, name, url, 0});

        serverMessage msg;
        msg.type = serverMessage::WS_PASS_MESSAGE;
        msg.singleClient = true;
        msg.value = client->id();
        snprintf(msg.str, sizeof(msg.str), "Added '%s' to playlist", name);
        xQueueSend(serverQueue, &msg, portMAX_DELAY);

        msg.singleClient = false;
        msg.type = serverMessage::WS_UPDATE_PLAYLIST;
        xQueueSend(serverQueue, &msg, portMAX_DELAY);

        const bool startnow = (pch[0] == '_');
        if (startnow || playList.currentItem() == PLAYLIST_STOPPED)
        {
            playerMessage msg;
            msg.action = playerMessage::START_ITEM;
            msg.value = playList.size() - 1;
            xQueueSend(playerQueue, &msg, portMAX_DELAY);
        }
    }

    else
    {
        log_w("unhandled single frame ws event! %s", pch);
    }
}

void handleMultiFrame(AsyncWebSocketClient *client, uint8_t *data, size_t len, AwsFrameInfo *info)
{
    static String message;

    message.concat(data, len); // todo: check result

    if ((info->index + len) == info->len && info->final)
    {
        log_d("Final multi frame message for %i bytes", info->index + len);
        if (message.startsWith("_filetoplaylist") || message.startsWith("filetoplaylist"))
        {
            const bool startnow = (message[0] == '_');
            const uint32_t previousSize = playList.size();

            auto pos = message.indexOf("\n");
            if (-1 == pos)
                return;
            pos++;

            auto number_of_urls{0};
            while (pos < info->len)
            {
                char url[PLAYLIST_MAX_URL_LENGTH];
                auto cnt = 0;
                while (message.charAt(pos) != '\n') /* TODO: BUG! possible buffer overflow */
                {
                    url[cnt++] = message.charAt(pos++);
                    if (pos == info->len)
                        return;
                }
                url[cnt] = 0;
                playList.add({HTTP_FILE, "", url, 0});
                number_of_urls++;
                pos++;
            }

            const uint32_t itemsAdded{playList.size() - previousSize};

            if (!itemsAdded)
            {
                serverMessage msg;
                msg.type = serverMessage::WS_PASS_MESSAGE;
                msg.singleClient = true;
                msg.value = client->id();
                snprintf(msg.str, sizeof(msg.str), "ERROR: Could not add items to playlist");
                xQueueSend(serverQueue, &msg, portMAX_DELAY);
                return;
            }

            updatePlaylistOverWebSocket();

            if (itemsAdded < number_of_urls)
            {
                serverMessage msg;
                msg.type = serverMessage::WS_PASS_MESSAGE;
                msg.singleClient = true;
                msg.value = client->id();
                snprintf(msg.str, sizeof(msg.str), "ERROR: Could only add %u of %i items", itemsAdded, number_of_urls);
                xQueueSend(serverQueue, &msg, portMAX_DELAY);
            }

            if (itemsAdded == number_of_urls)
            {
                serverMessage msg;
                msg.type = serverMessage::WS_PASS_MESSAGE;
                msg.singleClient = true;
                msg.value = client->id();
                snprintf(msg.str, sizeof(msg.str), "Added %u items to playlist", itemsAdded);
                xQueueSend(serverQueue, &msg, portMAX_DELAY);
            }

            if (startnow || playList.currentItem() == PLAYLIST_STOPPED)
            {
                playerMessage msg;
                msg.action = playerMessage::START_ITEM;
                msg.value = previousSize;
                xQueueSend(playerQueue, &msg, portMAX_DELAY);
            }
        }
        message.clear();
    }
}
