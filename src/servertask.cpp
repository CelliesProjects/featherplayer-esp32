#include "servertask.h"

static const char *HEADER_MODIFIED_SINCE = "If-Modified-Since";

static inline __attribute__((always_inline)) bool htmlUnmodified(PsychicRequest *request, const char *date)
{
    return request->hasHeader(HEADER_MODIFIED_SINCE) && request->header(HEADER_MODIFIED_SINCE).equals(date);
}

static const char *currentPlayingItem()
{
    static char buff[25];
    snprintf(buff, sizeof(buff), "currentPLitem\n%i\n", playList.currentItem());
    return buff;
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

static void webserverUrlSetup()
{
    time_t bootTime;
    time(&bootTime);
    static char modifiedDate[30];
    strftime(modifiedDate, sizeof(modifiedDate), "%a, %d %b %Y %X GMT", gmtime(&bootTime));

    static const char *HTML_MIMETYPE{"text/html"};
    static const char *HEADER_LASTMODIFIED{"Last-Modified"};
    static const char *HEADER_CONTENT_ENCODING{"Content-Encoding"};
    static const char *GZIP_CONTENT_ENCODING{"gzip"};

    server.on(
        "/", [](PsychicRequest *request)
        {
            if (htmlUnmodified(request, modifiedDate))   
                return request->reply(304);

            PsychicResponse response = PsychicResponse(request);
            response.addHeader(HEADER_LASTMODIFIED, modifiedDate);
            response.addHeader(HEADER_CONTENT_ENCODING, GZIP_CONTENT_ENCODING);
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

    constexpr const char *SVG_MIMETYPE{"image/svg+xml"};

    auto createIconURL = [&](const char *uri, const char *icon)
    {
        server.on(
            uri, [icon](PsychicRequest *request)
            {
                if (htmlUnmodified(request, modifiedDate))
                    return request->reply(304);

                PsychicResponse response = PsychicResponse(request);
                response.setContent(icon);
                response.setContentType(SVG_MIMETYPE);
                response.addHeader(HEADER_LASTMODIFIED, modifiedDate);
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
            return request->reply(404, HTML_MIMETYPE, "<h1>Aaaw please dont cry</h1>This is a 404 page<br>You reached the end of the road...<br>The page you are looking for is gone"); });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
}

static void wsNewClientHandler(PsychicWebSocketClient *client)
{
    log_i("[socket] connection #%u connected from %s", client->socket(), client->remoteIP().toString().c_str());

    client->sendMessage(streamTitle);
    client->sendMessage(showStation);

    char buff[20];
    snprintf(buff, sizeof(buff), "volume\n%i\n", _playerVolume);
    client->sendMessage(buff);

    snprintf(buff, sizeof(buff), "status\n%s\n", _paused ? "paused" : "playing");
    client->sendMessage(buff);

    String s;
    client->sendMessage(playList.toString(s).c_str());
    client->sendMessage(favoritesToString(s).c_str());
}

static esp_err_t wsFrameHandler(PsychicWebSocketRequest *request, httpd_ws_frame *frame)
{
    log_i("[socket] #%d sent: %s", request->client()->socket(), (char *)frame->payload);
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
