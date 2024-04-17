#include "servertask.h"

static const char *FAVORITES_FOLDER = "/"; /* if this is a folder use a closing slash */
static const char *VOLUME_HEADER = "volume";
static const char *HEADER_MODIFIED_SINCE = "If-Modified-Since";

static inline __attribute__((always_inline)) bool htmlUnmodified(const AsyncWebServerRequest *request, const char *date)
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

void callbackSetup(AsyncWebServer &server)
{
    time_t bootTime;
    time(&bootTime);
    static char modifiedDate[30];
    strftime(modifiedDate, sizeof(modifiedDate), "%a, %d %b %Y %X GMT", gmtime(&bootTime));

    static const char *HTML_MIMETYPE{"text/html"};
    static const char *HEADER_LASTMODIFIED{"Last-Modified"};
    static const char *HEADER_CONTENT_ENCODING{"Content-Encoding"};
    static const char *GZIP_CONTENT_ENCODING{"gzip"};

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, HTML_MIMETYPE, index_htm_gz, index_htm_gz_len);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        response->addHeader(HEADER_CONTENT_ENCODING, GZIP_CONTENT_ENCODING);
        request->send(response); });

    server.on("/scripturl", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncResponseStream* const response = request->beginResponseStream(HTML_MIMETYPE);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        response->println(SCRIPT_URL);
        if (strlen(LIBRARY_USER) || strlen(LIBRARY_PWD)) {
            response->println(LIBRARY_USER);
            response->println(LIBRARY_PWD);
        }
        request->send(response); });

    server.on("/stations", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncResponseStream* const response = request->beginResponseStream(HTML_MIMETYPE);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        auto i = 0;
        while (i < NUMBER_OF_PRESETS)
            response->printf("%s\n", preset[i++].name.c_str());
        request->send(response); });

    server.on("/favorites", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        AsyncResponseStream* const response = request->beginResponseStream("text/plain");
        response->addHeader("Cache-Control", "no-cache,no-store,must-revalidate,max-age=0");
        String s;
        response->print(favoritesToCStruct(s));
        request->send(response); });

    static const char *SVG_MIMETYPE{"image/svg+xml"};

    server.on("/radioicon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, radioicon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/playicon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, playicon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/libraryicon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, libraryicon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/favoriteicon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, favoriteicon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/streamicon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, pasteicon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/deleteicon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, deleteicon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/addfoldericon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, addfoldericon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/emptyicon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, emptyicon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/starticon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, starticon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/pauseicon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, pauseicon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/searchicon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, searchicon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.on("/nosslicon.svg", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        if (htmlUnmodified(request, modifiedDate)) return request->send(304);
        AsyncWebServerResponse* const response = request->beginResponse_P(200, SVG_MIMETYPE, nosslicon);
        response->addHeader(HEADER_LASTMODIFIED, modifiedDate);
        request->send(response); });

    server.onNotFound([](AsyncWebServerRequest *request)
                      {
        log_e("404 - Not found: 'http://%s%s'", request->host().c_str(), request->url().c_str());
        request->send(404); });

    DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
}

void serverTask(void *parameter)
{
    AsyncWebServer server(80);
    AsyncWebSocket ws("/ws");

    callbackSetup(server);
    server.begin();
    ws.onEvent(websocketEventHandler);
    server.addHandler(&ws);

    while (1)
    {
        static serverMessage msg{};
        if (xQueueReceive(serverQueue, &msg, pdMS_TO_TICKS(500)) == pdTRUE)
        {
            switch (msg.type)
            {
            case serverMessage::WS_UPDATE_NOWPLAYING:
                ws.textAll(currentPlayingItem());
                break;

            case serverMessage::WS_UPDATE_PLAYLIST:
            {
                String s;
                if (msg.singleClient)
                    ws.text(msg.value, playList.toString(s));
                else
                    ws.textAll(playList.toString(s));
                break;
            }

            case serverMessage::WS_PASS_MESSAGE:
                if (msg.singleClient)
                    ws.text(msg.value, msg.str);
                else
                    ws.textAll(msg.str);
                break;

            case serverMessage::WS_UPDATE_FAVORITES:
            {
                String s;
                if (msg.singleClient)
                    ws.text(msg.value, favoritesToString(s));
                else
                    ws.textAll(favoritesToString(s));
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
                ws.textAll(buff);
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
                // check the item type to show the tail end of url for real files
                snprintf(buff, sizeof(buff), "showstation\n%s\n%s\n", msg.str, typeStr[item.type]);
                ws.textAll(buff);
                break;
            }

            default:
                log_w("unhandled player message with number %i", msg.type);
            }
        }

        ws.cleanupClients();
        ws._cleanBuffers();
    }
}