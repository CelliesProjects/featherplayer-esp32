#include "servertask.h"

static const char *HEADER_MODIFIED_SINCE = "If-Modified-Since";

static inline __attribute__((always_inline)) bool htmlUnmodified(PsychicRequest *request, const char *date)
{
    return request->hasHeader(HEADER_MODIFIED_SINCE) && request->header(HEADER_MODIFIED_SINCE).equals(date);
}

void sendPlayerMessage(playerMessage::Type type, uint8_t value = 0, size_t offset = 0)
{
    static playerMessage msg;
    msg.type = type;
    msg.value = value;
    msg.offset = offset;
    xQueueSend(playerQueue, &msg, portMAX_DELAY);
}

void sendServerMessage(serverMessage::Type type, const char *str = "", bool singleClient = false, size_t value = 0)
{
    static serverMessage msg;
    msg.type = type;
    msg.singleClient = singleClient;
    msg.value = value;
    if (strlen(str))
        snprintf(msg.str, sizeof(msg.str), "%s", str);
    xQueueSend(serverQueue, &msg, portMAX_DELAY);
}

static const char *currentPlayingItem()
{
    static char buff[25];
    snprintf(buff, sizeof(buff), "currentPLitem\n%i\n", playList.currentItem());
    return buff;
}

static void handleFavoriteToPlaylist(PsychicRequest *request, const char *filename, const bool startNow)
{
    if (PLAYLIST_MAX_ITEMS == playList.size())
    {
        log_e("ERROR! Could not add %s to playlist", filename);

        char buff[256]{};
        snprintf(buff, 256, "ERROR: Could not add '%s' to playlist", filename);
        sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, request->client()->socket());
        return;
    }

    char path[strlen(FAVORITES_FOLDER) + strlen(filename) + 1];
    snprintf(path, sizeof(path), "%s%s", FAVORITES_FOLDER, filename);
    File file = FFat.open(path);
    if (!file)
    {
        log_e("ERROR! Could not open %s", filename);

        char buff[256]{};
        snprintf(buff, 256, "ERROR: Could not add '%s' to playlist", filename);
        sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, request->client()->socket());
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
    url[cnt] = '\0';
    file.close();

    const auto previousSize = playList.size();
    playList.add({HTTP_FAVORITE, filename, url, 0});
    log_d("favorite to playlist: %s -> %s", filename, url);

    if (startNow || playList.currentItem() == PLAYLIST_STOPPED)
    {
        playList.setCurrentItem(previousSize);
        sendPlayerMessage(playerMessage::START_ITEM, playList.currentItem());
    }
}

static const String &favoritesToCStruct(String &s)
{
    File folder = FFat.open(FAVORITES_FOLDER);
    if (!folder)
    {
        s = "ERROR! Could not open folder " + String(FAVORITES_FOLDER);
        return s;
    }
    s = "const source preset[] = {\n";
    File file = folder.openNextFile();
    while (file)
    {
        if (!file.isDirectory() && file.size() < PLAYLIST_MAX_URL_LENGTH)
        {
            s.concat("    {\"");
            s.concat(file.name());
            s.concat("\", \"");
            char ch = (char)file.read();
            while (file.available() && ch != '\n')
            {
                s.concat(ch);
                ch = (char)file.read();
            }
            s.concat("\"},\n");
        }
        file.close();
        file = folder.openNextFile();
    }
    folder.close();
    s.concat("};\n");
    return s;
}

static const String &favoritesToString(String &s)
{
    s = "favorites\n";
    File folder = FFat.open(FAVORITES_FOLDER);
    if (!folder)
    {
        log_e("ERROR! Could not open favorites folder");
        return s;
    }
    File file = folder.openNextFile();
    while (file)
    {
        if (!file.isDirectory() && file.size() < PLAYLIST_MAX_URL_LENGTH)
        {
            s.concat(file.name());
            s.concat("\n");
        }
        file.close();
        file = folder.openNextFile();
    }
    folder.close();
    return s;
}

static bool saveItemToFavorites(PsychicWebSocketClient *client, const char *filename, const playListItem &item)
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

            char buff[256]{};
            snprintf(buff, 256, "ERROR: Could not open '%s' for writing!", filename);
            sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, client->socket());
            return false;
        }
        char url[item.url.length() + 2];
        snprintf(url, sizeof(url), "%s\n", item.url.c_str());
        const auto bytesWritten = file.print(url);
        file.close();
        if (bytesWritten < strlen(url))
        {
            log_e("ERROR! Saving '%s' failed - disk full?", filename);

            char buff[256]{};
            snprintf(buff, 256, "ERROR: Could not completely save '%s' to favorites!", filename);
            sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, client->socket());
            return false;
        }
        char buff[256]{};
        snprintf(buff, 256, "Saved '%s' to favorites!", filename);
        sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, client->socket());
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

static void webserverUrlSetup()
{
    time_t bootTime;
    time(&bootTime);
    static char modifiedDate[30];
    strftime(modifiedDate, sizeof(modifiedDate), "%a, %d %b %Y %X GMT", gmtime(&bootTime));

    constexpr const char *MIMETYPE_HTML{"text/html"};
    constexpr const char *HEADER_LASTMODIFIED{"Last-Modified"};
    constexpr const char *HEADER_CONTENT_ENCODING{"Content-Encoding"};
    constexpr const char *CONTENT_ENCODING_GZIP{"gzip"};

    server.on(
        "/", [](PsychicRequest *request)
        {
            if (htmlUnmodified(request, modifiedDate))   
                return request->reply(304);

            PsychicResponse response = PsychicResponse(request);
            response.addHeader(HEADER_LASTMODIFIED, modifiedDate);
            response.addHeader(HEADER_CONTENT_ENCODING, CONTENT_ENCODING_GZIP);
            response.setContent(index_htm_gz, index_htm_gz_len);
            return response.send(); });

    server.on(
        "/scripturl", [](PsychicRequest *request)
        {
            if (htmlUnmodified(request, modifiedDate))
                return request->reply(304);

            String content = SCRIPT_URL;
            content.concat("\n");
            if (strlen(LIBRARY_USER) || strlen(LIBRARY_PWD))
            {
                content.concat(LIBRARY_USER);
                content.concat("\n");
                content.concat(LIBRARY_PWD);
                content.concat("\n");
            }
            PsychicResponse response = PsychicResponse(request);
            response.addHeader(HEADER_LASTMODIFIED, modifiedDate);  
            response.setContent(content.c_str());
            return response.send(); });

    server.on(
        "/stations", [](PsychicRequest *request)
        {
            if (htmlUnmodified(request, modifiedDate))
                return request->reply(304);

            String content;
            int i = 0;
            while (i < NUMBER_OF_PRESETS)
            {
                content.concat(preset[i++].name);
                content.concat("\n");
            }
            PsychicResponse response = PsychicResponse(request);
            response.addHeader(HEADER_LASTMODIFIED, modifiedDate);
            response.setContent(content.c_str());
            return response.send(); });

    server.on(
        "/favorites", [](PsychicRequest *request)
        {
            String content = favoritesToCStruct(content);
            PsychicResponse response = PsychicResponse(request);
            response.addHeader("Cache-Control", "no-cache,no-store,must-revalidate,max-age=0");
            response.setContent(content.c_str());
            return response.send(); });

    constexpr const char *MIMETYPE_SVG{"image/svg+xml"};

    auto createIconURL = [&](const char *uri, const char *icon)
    {
        server.on(
            uri, [icon](PsychicRequest *request)
            {
                if (htmlUnmodified(request, modifiedDate))
                    return request->reply(304);

                PsychicResponse response = PsychicResponse(request);
                response.setContent(icon);
                response.setContentType(MIMETYPE_SVG);
                response.addHeader(HEADER_LASTMODIFIED, modifiedDate);
                response.addHeader("Cache-Control", "public, max-age=31536000");
                return response.send(); });
    };

    createIconURL("/radioicon.svg", radioicon);
    createIconURL("/playicon.svg", playicon);
    createIconURL("/libraryicon.svg", libraryicon);
    createIconURL("/favoriteicon.svg", favoriteicon);
    createIconURL("/pasteicon.svg", pasteicon);
    createIconURL("/deleteicon.svg", deleteicon);
    createIconURL("/addfoldericon.svg", addfoldericon);
    createIconURL("/emptyicon.svg", emptyicon);
    createIconURL("/starticon.svg", starticon);
    createIconURL("/pauseicon.svg", pauseicon);
    createIconURL("/searchicon.svg", searchicon);
    createIconURL("/nosslicon.svg", nosslicon);

    server.onNotFound(
        [](PsychicRequest *request)
        {
            log_e("404 - Not found: 'http://%s%s'", request->host().c_str(), request->url().c_str());
            return request->reply(404, MIMETYPE_HTML, "<h1>Aaaw please dont cry</h1>This is a 404 page<br>You reached the end of the road...<br>The page you are looking for is gone"); });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
}

static void wsNewClientHandler(PsychicWebSocketClient *client)
{
    log_i("[socket] connection #%u connected from %s", client->socket(), client->remoteIP().toString().c_str());

    client->sendMessage(streamTitle);
    client->sendMessage(showStation);

    char buff[32];
    snprintf(buff, sizeof(buff), "volume\n%i\n", _playerVolume);
    client->sendMessage(buff);

    snprintf(buff, sizeof(buff), "status\n%s\n", _paused ? "paused" : "playing");
    client->sendMessage(buff);

    String s;
    client->sendMessage(playList.toString(s).c_str());
    client->sendMessage(favoritesToString(s).c_str());
}

static void updatePlaylistOverWebSocket()
{
    serverMessage msg;
    msg.type = serverMessage::WS_UPDATE_PLAYLIST;
    xQueueSend(serverQueue, &msg, portMAX_DELAY);
}

static esp_err_t wsFrameHandler(PsychicWebSocketRequest *request, httpd_ws_frame *frame)
{
    log_v("received payload: %s", reinterpret_cast<char *>(frame->payload));

    if (frame->type != HTTPD_WS_TYPE_TEXT)
    {
        log_w("frame type != text");
        return ESP_OK;
    }
    char *pch = strtok(reinterpret_cast<char *>(frame->payload), "\n");
    if (!pch)
    {
        log_w("no payload");
        return ESP_OK;
    }

    if (_paused && !strcmp("unpause", pch))
    {
        sendPlayerMessage(playerMessage::START_ITEM, playList.currentItem(), _savedPosition);
        return ESP_OK;
    }

    else if (!_paused && !strcmp("pause", pch))
    {
        _paused = true;
        sendPlayerMessage(playerMessage::PAUSE);
        sendServerMessage(serverMessage::WS_UPDATE_STATUS, "paused");
        return ESP_OK;
    }

    else if (!strcmp("volume", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return ESP_OK;
        const uint8_t volume = atoi(pch);
        sendPlayerMessage(playerMessage::SET_VOLUME,
                          volume > VS1053_MAXVOLUME ? VS1053_MAXVOLUME : volume);
        return ESP_OK;
    }

    else if (!strcmp("previous", pch))
    {
        if (playList.currentItem() == PLAYLIST_STOPPED)
            return ESP_OK;

        if (playList.currentItem() > 0)
            sendPlayerMessage(playerMessage::START_ITEM, playList.currentItem() - 1);
        return ESP_OK;
    }

    else if (!strcmp("next", pch))
    {
        if (playList.currentItem() == PLAYLIST_STOPPED)
            return ESP_OK;

        if (playList.currentItem() < playList.size() - 1)
            sendPlayerMessage(playerMessage::START_ITEM, playList.currentItem() + 1);
        return ESP_OK;
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
            return ESP_OK;

        if (itemsAdded > 1)
        {
            char buff[32];
            snprintf(buff, 32, "Added %u items to playlist", itemsAdded);
            sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, request->client()->socket());
        }
        updatePlaylistOverWebSocket();

        if (startnow || playList.currentItem() == PLAYLIST_STOPPED)
            sendPlayerMessage(playerMessage::START_ITEM, previousSize);
        return ESP_OK;
    }

    else if (!strcmp("playitem", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return ESP_OK;

        const uint8_t index = atoi(pch);
        if (index < playList.size())
            sendPlayerMessage(playerMessage::START_ITEM, index);
        return ESP_OK;
    }

    else if (!strcmp("deleteitem", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return ESP_OK;

        const uint8_t index = atoi(pch);
        if (index >= playList.size())
            return ESP_OK;

        playList.remove(index);

        if (index < playList.currentItem())
            playList.setCurrentItem(playList.currentItem() - 1);

        else if (playList.currentItem() == index)
        {
            if (playList.currentItem() < playList.size())
                sendPlayerMessage(playerMessage::START_ITEM, playList.currentItem());
            else
            {
                sendPlayerMessage(playerMessage::STOPSONG);
                playListEnd();
            }
        }
        updatePlaylistOverWebSocket();
        return ESP_OK;
    }

    else if (!strcmp("clearlist", pch))
    {
        if (!playList.size())
            return ESP_OK;

        sendPlayerMessage(playerMessage::STOPSONG);

        playList.clear();
        updatePlaylistOverWebSocket();
        playListEnd();
        return ESP_OK;
    }

    else if (!strcmp("presetstation", pch) || !strcmp("_presetstation", pch))
    {
        log_d("adding preset");
        const bool startnow = (pch[0] == '_');
        pch = strtok(NULL, "\n");
        if (!pch)
            return ESP_OK;

        const uint32_t index = atoi(pch);
        if (index >= NUMBER_OF_PRESETS)
            return ESP_OK;

        const uint32_t previousSize = playList.size();
        playList.add({HTTP_PRESET, "", "", index});
        if (playList.size() == previousSize)
        {
            char buff[256];
            snprintf(buff, 256, "ERROR: Could not add '%s' to playlist", preset[index].name.c_str());
            sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, request->client()->socket());
            return ESP_OK;
        }

        log_d("Added '%s' to playlist", preset[index].name.c_str());

        updatePlaylistOverWebSocket();

        if (startnow || playList.currentItem() == PLAYLIST_STOPPED)
            sendPlayerMessage(playerMessage::START_ITEM, playList.size() - 1);

        return ESP_OK;
    }

    else if (!strcmp("jumptopos", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return ESP_OK;

        sendPlayerMessage(playerMessage::START_ITEM, playList.currentItem(), atoi(pch));
        return ESP_OK;
    }

    else if (!strcmp("currenttofavorites", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return ESP_OK;

        playListItem item;
        playList.get(playList.currentItem(), item);
        if (saveItemToFavorites(request->client(), pch, item))
            sendServerMessage(serverMessage::WS_UPDATE_FAVORITES);
        else
        {
            char buff[256];
            snprintf(buff, 256, "ERROR: Could not add '%s' to playlist!", pch);
            sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, request->client()->socket());
        }
        return ESP_OK;
    }

    else if (!strcmp("favoritetoplaylist", pch) || !strcmp("_favoritetoplaylist", pch))
    {
        const bool startNow = (pch[0] == '_');
        pch = strtok(NULL, "\n");
        if (!pch)
            return ESP_OK;

        if (playList.size() == PLAYLIST_MAX_ITEMS)
        {
            char buff[256];
            snprintf(buff, 256, "ERROR: Could not add '%s' to playlist!", pch);
            sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, request->client()->socket());
            return ESP_OK;
        }

        const auto cnt = playList.size();
        handleFavoriteToPlaylist(request, pch, startNow);
        if (playList.size() > cnt)
            updatePlaylistOverWebSocket();
        return ESP_OK;
    }

    else if (!strcmp("deletefavorite", pch))
    {
        pch = strtok(NULL, "\n");
        if (!pch)
            return ESP_OK;

        char filename[strlen(FAVORITES_FOLDER) + strlen(pch) + 1];
        snprintf(filename, sizeof(filename), "%s%s", FAVORITES_FOLDER, pch);
        if (!FFat.remove(filename))
        {
            char buff[256];
            snprintf(buff, 256, "ERROR: Could not delete %s", filename);
            sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, request->client()->socket());
        }
        else
            sendServerMessage(serverMessage::WS_UPDATE_FAVORITES);
        return ESP_OK;
    }

    else if (!strcmp("foundlink", pch) || !strcmp("_foundlink", pch))
    {
        if (playList.size() == PLAYLIST_MAX_ITEMS)
        {
            sendServerMessage(serverMessage::WS_PASS_MESSAGE,
                              "ERROR: Could not add new url to playlist", true, request->client()->socket());
            return ESP_OK;
        }
        
        const char *url = strtok(NULL, "\n");
        if (!url)
            return ESP_OK;

        const char *name = strtok(NULL, "\n");
        if (!name)
            return ESP_OK;

        playList.add({HTTP_FOUND, name, url, 0});

        sendServerMessage(serverMessage::WS_UPDATE_PLAYLIST);

        char buff[256];
        snprintf(buff, 256, "Added '%s' to playlist", name);
        sendServerMessage(serverMessage::WS_PASS_MESSAGE, buff, true, request->client()->socket());

        const bool startnow = (pch[0] == '_');
        if (startnow || playList.currentItem() == PLAYLIST_STOPPED)
            sendPlayerMessage(playerMessage::START_ITEM, playList.size() - 1);
        return ESP_OK;
    }

    else
        log_i("unhandled payload: %s", frame->payload);

    return ESP_OK;
}

void serverTask(void *parameter)
{
    server.config.max_uri_handlers = 25;
    server.config.max_open_sockets = 10;

    server.listen(80);
    webserverUrlSetup();

    websocketHandler.onOpen(wsNewClientHandler);
    websocketHandler.onFrame(wsFrameHandler);

    server.on("/ws", &websocketHandler);

    while (1)
    {
        static serverMessage msg{};
        if (xQueueReceive(serverQueue, &msg, portMAX_DELAY) == pdTRUE)
        {
            switch (msg.type)
            {
            case serverMessage::WS_UPDATE_NOWPLAYING:
                websocketHandler.sendAll(currentPlayingItem());
                break;

            case serverMessage::WS_UPDATE_PLAYLIST:
            {
                String s;
                websocketHandler.sendAll(playList.toString(s).c_str());
                break;
            }

            case serverMessage::WS_PASS_MESSAGE:
                char buff[256];
                snprintf(buff, sizeof(buff), "message\n%s", msg.str);
                if (msg.singleClient)
                {
                    PsychicWebSocketClient *client = websocketHandler.getClient(msg.value);
                    if (client != NULL)
                        client->sendMessage(buff);
                    break;
                }
                websocketHandler.sendAll(buff);
                break;

            case serverMessage::WS_UPDATE_FAVORITES:
            {
                String s;
                websocketHandler.sendAll(favoritesToString(s).c_str());
                break;
            }

            case serverMessage::WS_UPDATE_STREAMTITLE:
            {
                snprintf(streamTitle, sizeof(streamTitle), "streamtitle\n%s\n", percentEncode(msg.str).c_str());
                websocketHandler.sendAll(streamTitle);
                break;
            }

            case serverMessage::WS_UPDATE_VOLUME:
            {
                char buff[20];
                snprintf(buff, sizeof(buff), "volume\n%i\n", _playerVolume);
                websocketHandler.sendAll(buff);
                break;
            }

            case serverMessage::WS_UPDATE_STATION:
            {
                playListItem item;
                playList.get(playList.currentItem(), item);
                snprintf(showStation, sizeof(showStation), "showstation\n%s\n%s\n", msg.str, typeStr[item.type]);
                websocketHandler.sendAll(showStation);
                break;
            }

            case serverMessage::WS_UPDATE_PROGRESS:
            {
                char buff[48];
                snprintf(buff, sizeof(buff), "progress\n%i\n%i\n", msg.value, msg.value2);
                websocketHandler.sendAll(buff);
                break;
            }

            case serverMessage::WS_UPDATE_STATUS:
            {
                char buff[30];
                snprintf(buff, sizeof(buff), "status\n%s\n", msg.str);
                websocketHandler.sendAll(buff);
                break;
            }
            default:
                log_w("unhandled player message with number %i", msg.type);
            }
        }
    }
}
