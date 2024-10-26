#include "servertask.h"

static const char *HEADER_MODIFIED_SINCE = "If-Modified-Since";

static inline __attribute__((always_inline)) bool htmlUnmodified(PsychicRequest *request, const char *date)
{
    return request->hasHeader(HEADER_MODIFIED_SINCE) && request->header(HEADER_MODIFIED_SINCE).equals(date);
}

const char *currentPlayingItem()
{
    static char buff[25];
    snprintf(buff, sizeof(buff), "currentPLitem\n%i\n", playList.currentItem());
    return buff;
}

const String &favoritesToCStruct(String &s)
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
        file = folder.openNextFile();
    }
    s.concat("};\n");
    return s;
}

const String &favoritesToString(String &s)
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
        file = folder.openNextFile();
    }
    return s;
}

void callbackSetup()
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
        if (htmlUnmodified(request, modifiedDate)) return request->reply(304);
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
        // request->header(HEADER_LASTMODIFIED, modifiedDate);
        return request->reply(content.c_str()); });

    server.on(
        "/stations", [](PsychicRequest *request)
        {
        if (htmlUnmodified(request, modifiedDate))
            return request->reply(304);

        PsychicResponse response = PsychicResponse(request);
        response.addHeader(HEADER_LASTMODIFIED, modifiedDate);
        String content;
        int i = 0;
        while (i < NUMBER_OF_PRESETS)
            {
                content.concat(preset[i++].name);
                content.concat("\n");
            }
        response.setContent(content.c_str());
        return response.send(); });

    server.on(
        "/favorites", [](PsychicRequest *request)
        {
        PsychicResponse response = PsychicResponse(request);
        response.addHeader("Cache-Control", "no-cache,no-store,must-revalidate,max-age=0");
        String content = favoritesToCStruct(content);
        response.setContent(content.c_str());
        return response.send(); });

    static const char *SVG_MIMETYPE{"image/svg+xml"};

    auto createIconRoute = [&](const char *uri, const char *icon)
    {
        server.on(uri, [=](PsychicRequest *request)
                  {
            if (htmlUnmodified(request, modifiedDate))
                return request->reply(304);

            PsychicResponse response = PsychicResponse(request);
            response.setContent(icon);
            response.setContentType(SVG_MIMETYPE);
            response.addHeader(HEADER_LASTMODIFIED, modifiedDate);

            return response.send(); });
    };

    // Initialize routes for each icon
    createIconRoute("/radioicon.svg", radioicon);
    createIconRoute("/playicon.svg", playicon);
    createIconRoute("/libraryicon.svg", libraryicon);
    createIconRoute("/favoriteicon.svg", favoriteicon);
    createIconRoute("/pasteicon.svg", pasteicon);
    createIconRoute("/deleteicon.svg", deleteicon);
    createIconRoute("/addfoldericon.svg", addfoldericon);
    createIconRoute("/emptyicon.svg", emptyicon);
    createIconRoute("/starticon.svg", starticon);
    createIconRoute("/pauseicon.svg", pauseicon);
    createIconRoute("/searchicon.svg", searchicon);
    createIconRoute("/nosslicon.svg", nosslicon);

    server.onNotFound(
        [](PsychicRequest *request)
        {
        log_e("404 - Not found: 'http://%s%s'", request->host().c_str(), request->url().c_str());
        return request->reply(404); });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
}

void serverTask(void *parameter)
{
    server.config.max_uri_handlers = 20;
    server.listen(80);

    callbackSetup();

    websocketHandler.onFrame(
        [](PsychicWebSocketRequest *request, httpd_ws_frame *frame)
        {
     Serial.printf("[socket] #%d sent: %s\n", request->client()->socket(), (char *)frame->payload);
     return request->reply(frame); });

    server.on("/ws",&websocketHandler);

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
                if (msg.singleClient)
                    ws.text(msg.value, playList.toString(s));
                else
                    websocketHandler.sendAll(playList.toString(s));
                break;
            }

            case serverMessage::WS_PASS_MESSAGE:
                char buff[256];
                snprintf(buff, sizeof(buff), "message\n%s", msg.str);
                if (msg.singleClient)
                    ws.text(msg.value, buff);
                else
                    websocketHandler.sendAll(buff);
                break;

            case serverMessage::WS_UPDATE_FAVORITES:
            {
                String s;
                if (msg.singleClient)
                    ws.text(msg.value, favoritesToString(s));
                else
                    websocketHandler.sendAll(favoritesToString(s));
                break;
            }

            case serverMessage::WS_UPDATE_STREAMTITLE:
            {
                static char buff[300]{};
                if (msg.singleClient)
                {
                    ws.text(msg.value, buff);
                    break;
                }
                snprintf(buff, sizeof(buff), "streamtitle\n%s\n", percentEncode(msg.str).c_str());
                websocketHandler.sendAll(buff);
                break;
            }

            case serverMessage::WS_UPDATE_VOLUME:
            {
                char buff[20];
                snprintf(buff, sizeof(buff), "volume\n%i\n", _playerVolume);
                if (msg.singleClient)
                    ws.text(msg.value, buff);
                else
                    websocketHandler.sendAll(buff);
                break;
            }

            case serverMessage::WS_UPDATE_STATION:
            {
                static char buff[300]{};
                if (msg.singleClient)
                {
                    ws.text(msg.value, buff);
                    break;
                }
                playListItem item;
                playList.get(playList.currentItem(), item);
                snprintf(buff, sizeof(buff), "showstation\n%s\n%s\n", msg.str, typeStr[item.type]);
                websocketHandler.sendAll(buff);
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
                char buff[168];
                snprintf(buff, sizeof(buff), "status\n%s\n", msg.str);
                if (msg.singleClient)
                    ws.text(msg.value, buff);
                else
                    websocketHandler.sendAll(buff);
                break;
            }
            default:
                log_w("unhandled player message with number %i", msg.type);
            }
            //ws.cleanupClients();
        }
    }
}