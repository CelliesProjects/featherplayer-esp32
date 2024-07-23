#include "servertask.h"

static void mountSDcard()
{
    if (!SD.begin(SDREADER_CS, SPI, 20000000))
    {
        log_e("Card Mount Failed");
        return;
    }
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE)
    {
        log_e("No SD card attached");
        return;
    }

    log_i("SD Card Type: ");
    if (cardType == CARD_MMC)
        log_i("MMC");
    else if (cardType == CARD_SD)
        log_i("SDSC");
    else if (cardType == CARD_SDHC)
        log_i("SDHC");
    else
        log_i("UNKNOWN");

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    log_i("SD Card Size: %lluMB\n", cardSize);
}

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

static void sendFolderWS(File &folder, const serverMessage &msg, AsyncWebSocket &ws)
{
    if (!folder)
        return;
    log_i("folder scan '%s'", msg.str);
    const auto START = millis();
    String filename;
    bool isDir = false;
    xSemaphoreTake(spiMutex, portMAX_DELAY);
    filename = folder.getNextFileName(&isDir);
    xSemaphoreGive(spiMutex);

    auto cnt = 0;
    String response = "files-";
    response.concat(msg.str);
    response.concat("\n");

    // add a go-to-upper-dir-link
    const char *goUpItem = R"=====(<div id="uplink"><svg class="icon" xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24"><path fill="none" d="M0 0h24v24H0V0z"></path><path d="M11 9l1.42 1.42L8.83 14H18V4h2v12H8.83l3.59 3.58L11 21l-6-6 6-6z"></path></svg></div>)=====";
    response.concat(goUpItem);

    response.concat("\n\n\n"); // TODO : REMOVE

    while (filename != "")
    {
        log_d("name: %s", filename);
        cnt++;
        bool isPlayable = false;
        if (isDir)
        {
            xSemaphoreTake(spiMutex, portMAX_DELAY);
            File innerFolder = SD.open(filename);
            xSemaphoreGive(spiMutex);

            if (!innerFolder)
            {
                log_e("something went wrong scanning %s", filename);
                // send some form of error message to the ws client
                return;
            }

            // folder exists and contains something

            String current;
            bool innerIsDir = false;
            xSemaphoreTake(spiMutex, portMAX_DELAY);
            current = innerFolder.getNextFileName(&innerIsDir); // https://github.com/espressif/arduino-esp32/pull/7229
            xSemaphoreGive(spiMutex);

            while (current != "" && !isPlayable)
            {
                current.toLowerCase();
                isPlayable = !innerIsDir &&
                             (current.endsWith(".mp3") || current.endsWith(".ogg"));
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                current = innerFolder.getNextFileName(&innerIsDir);
                xSemaphoreGive(spiMutex);
            }
            innerFolder.close();
        }

        // check of er in de folder iets afspeelbaars staat
        // en laat ADDFOLDER_ICON en START_ICON zien voor de filename als dat het geval is
        // en anders 2 keer een emptyicon
        // het path gaat naar de www-data dingetje

        // DOE DE FOLDERS SEPERATE VAN DE AFSPEELBARE BESTANDEN
        if (isDir)
        {
            response.concat("<div class=\"folderlink\">");
            response.concat(isPlayable ? addfoldericon : emptyicon);
            response.concat(isPlayable ? starticon : emptyicon);
            response.concat("<span class=\"text\">");
            response.concat(filename.substring(filename.lastIndexOf('/') + 1));
            response.concat("</span></div>");
            response.concat("\n\n\n"); // TODO: REMOVE!
        }

        String test = "";
        if (!isDir && filename.length() > 4)
            test = filename.substring(filename.length() - 4);

        if (!isDir && (test.equalsIgnoreCase(".mp3") || test.equalsIgnoreCase(".ogg")))
        {
            const char *playableItemIcon = R"=====(<svg class="icon" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 0 24 24" width="24"><path d="M0 0h24v24H0z" fill="none"></path><path d="M12 3v9.28c-.47-.17-.97-.28-1.5-.28C8.01 12 6 14.01 6 16.5S8.01 21 10.5 21c2.31 0 4.2-1.75 4.45-4H15V6h4V3h-7z"></path></svg>)=====";
            const char *startItemNowIcon = R"=====(<img src="/starticon.svg" class="icon starticon">)=====";
            response.concat("<div id=\"filelink\">");
            response.concat(playableItemIcon);
            response.concat(startItemNowIcon);
            response.concat("<span class=\"text\"");
            response.concat(filename.substring(filename.lastIndexOf('/') + 1));
            response.concat("</span></div>");
            response.concat("\n\n\n"); // TODO: REMOVE!
        }

        xSemaphoreTake(spiMutex, portMAX_DELAY);
        filename = folder.getNextFileName(&isDir);
        xSemaphoreGive(spiMutex);
    }
    log_i("scanned %i files in %i ms", cnt, millis() - START);
    log_i("response: %s", response.c_str());

    ws.text(msg.value, response.c_str());
}

void serverTask(void *parameter)
{
    xSemaphoreTake(spiMutex, portMAX_DELAY);
    mountSDcard();
    xSemaphoreGive(spiMutex);

    static AsyncWebServer server(80);
    static AsyncWebSocket ws("/ws");

    callbackSetup(server);
    server.begin();
    ws.onEvent(websocketEventHandler);
    server.addHandler(&ws);

    // test: add a command to the server queue to show a folder
    serverMessage msg;
//    snprintf(msg.str, sizeof(msg.str), "/Front 242");
    snprintf(msg.str, sizeof(msg.str), "/DJ sets");
    //  snprintf(msg.str, sizeof(msg.str), "/De Jeugd van Tegenwoordig/Parels Voor De Zwijnen");
    //     snprintf(msg.str, sizeof(msg.str), "/allentoussaint");
    // snprintf(msg.str, sizeof(msg.str), "/");
    msg.type = serverMessage::WS_LIST_FOLDER;
    xQueueSend(serverQueue, &msg, portMAX_DELAY);

    while (1)
    {
        static serverMessage msg{};
        if (xQueueReceive(serverQueue, &msg, portMAX_DELAY) == pdTRUE)
        {
            switch (msg.type)
            {
            case serverMessage::WS_LIST_FOLDER:
            {
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                File folder = SD.open(msg.str);
                xSemaphoreGive(spiMutex);
                if (!folder)
                {
                    log_e("%s not found", msg.str);
                    break;
                }
                if (!folder.isDirectory())
                {
                    log_e("%s is no directory", msg.str);
                    folder.close();
                    return;
                }

                sendFolderWS(folder, msg, ws);
                folder.close();
            }
            break;

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
                char buff[256];
                snprintf(buff, sizeof(buff), "message\n%s", msg.str);
                if (msg.singleClient)
                    ws.text(msg.value, buff);
                else
                    ws.textAll(buff);
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

            case serverMessage::WS_UPDATE_VOLUME:
            {
                char buff[20];
                snprintf(buff, sizeof(buff), "volume\n%i\n", _playerVolume);
                if (msg.singleClient)
                    ws.text(msg.value, buff);
                else
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
                snprintf(buff, sizeof(buff), "showstation\n%s\n%s\n", msg.str, typeStr[item.type]);
                ws.textAll(buff);
                break;
            }

            case serverMessage::WS_UPDATE_PROGRESS:
            {
                char buff[48];
                snprintf(buff, sizeof(buff), "progress\n%i\n%i\n", msg.value, msg.value2);
                ws.textAll(buff);
                break;
            }

            case serverMessage::WS_UPDATE_STATUS:
            {
                char buff[168];
                snprintf(buff, sizeof(buff), "status\n%s\n", msg.str);
                if (msg.singleClient)
                    ws.text(msg.value, buff);
                else
                    ws.textAll(buff);
                break;
            }
            default:
                log_w("unhandled player message with number %i", msg.type);
            }
            ws.cleanupClients();
        }
    }
}