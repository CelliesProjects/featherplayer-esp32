#include "tfttask.hpp"

void sendTftMessage(tftMessage::Type type, const char *str = NULL, size_t value1 = 0, size_t value2 = 0)
{
    tftMessage msg;
    msg.type = type;
    msg.value1 = value1;
    msg.value2 = value2;
    if (str)
        snprintf(msg.str, sizeof(msg.str), "%s", str);
    xQueueSend(tftQueue, &msg, portMAX_DELAY);
}

float map_range(const float input,
                const float input_start, const float input_end,
                const float output_start, const float output_end)
{
    const float input_range = input_end - input_start;
    const float output_range = output_end - output_start;
    return (input - input_start) * (output_range / input_range) + output_start;
}

void handleSysMess(tftMessage &msg)
{
    canvas.fillScreen(lgfx::color565(255, 0, 0));
    canvas.setTextColor(lgfx::color565(255, 255, 255));
    canvas.setFont(&FreeSansBold9pt7b);
    canvas.setTextSize(1);
    canvas.setCursor(4, canvas.height() - canvas.fontHeight());
    canvas.print(msg.str);
    {
        ScopedMutex lock(spiMutex);
        canvas.pushSprite(0, 0);
    }
    if (playerTaskHandle)
        xTaskNotifyGive(playerTaskHandle);
}

void handleProgress(tftMessage &msg)
{
    const int HEIGHT_IN_PIXELS = 15;
    const int HEIGHT_OFFSET = 28;
    const int16_t FILLED_AREA = map_range(msg.value1, 0, msg.value2, 0, tft.width() + 1);

    ScopedMutex lock(spiMutex);
    tft.fillRect(0, HEIGHT_OFFSET, FILLED_AREA, HEIGHT_IN_PIXELS, lgfx::color565(0, 0, 255));
    tft.fillRect(FILLED_AREA, HEIGHT_OFFSET, tft.width() - FILLED_AREA, HEIGHT_IN_PIXELS, lgfx::color565(255, 255, 255));
}

void handleStation(tftMessage &msg)
{
    canvas.fillScreen(0);
    canvas.setFont(&FreeSansBold9pt7b);
    canvas.setTextSize(1);
    canvas.setCursor(4, canvas.height() - canvas.fontHeight());
    canvas.print(msg.str);

    ScopedMutex lock(spiMutex);
    canvas.pushSprite(0, 0);
}

void handleTitle(tftMessage &msg)
{
    if (!strcmp(streamTitle, msg.str))
        return;

    if (msg.str[0] == 0)
    {
        clearTitle = true;
        streamTitle[0] = 0;
        return;
    }
    streamTitleOffset = 0;
    snprintf(streamTitle, sizeof(streamTitle), "%s", msg.str);
    tft.setTextSize(1);
    tft.setFont(&FreeSansBold9pt7b);
    streamTitleWidth = tft.textWidth(streamTitle);
}

void handleBitrate(tftMessage &msg)
{
    constexpr int16_t spriteWidth = 60;
    constexpr int16_t spriteHeight = 20;
    const auto font = &DejaVu18;

    static LGFX_Sprite bitrate(&tft);
    if (!bitrate.getBuffer() && !bitrate.createSprite(spriteWidth, spriteHeight))
    {
        log_e("could not allocate canvas for bitrate. system halted");
        while (1)
            delay(100);
    }
    bitrate.clear();
    bitrate.setTextDatum(CC_DATUM);
    char br[8];
    snprintf(br, sizeof(br), "%zu", msg.value1);
    bitrate.drawCenterString(br, spriteWidth / 2, (spriteHeight / 2) - (font->yAdvance / 2), font);

    ScopedMutex lock(spiMutex);
    bitrate.pushSprite(70, 50);
}

void handleCodec(tftMessage &msg)
{
    constexpr int16_t spriteWidth = 60;
    constexpr int16_t spriteHeight = 20;
    const auto font = &DejaVu18;

    static LGFX_Sprite codec(&tft);
    if (!codec.getBuffer() && !codec.createSprite(spriteWidth, spriteHeight))
    {
        log_e("could not allocate canvas for codec. system halted");
        while (1)
            delay(100);
    }
    codec.clear();
    codec.setTextDatum(CC_DATUM);
    codec.drawCenterString(msg.str, spriteWidth / 2, (spriteHeight / 2) - (font->yAdvance / 2), font);

    ScopedMutex lock(spiMutex);
    codec.pushSprite(5, 50);
}

void handleIp(tftMessage &msg)
{
    constexpr int16_t spriteHeight = 40;
    static LGFX_Sprite ipAddressCanvas(&tft);
    if (!ipAddressCanvas.getBuffer() && !ipAddressCanvas.createSprite(tft.width(), spriteHeight))
    {
        log_e("could not allocate canvas for ipaddress. system halted");
        while (1)
            delay(100);
    }

    ipAddressCanvas.fillScreen(BACKGROUND_COLOR);
    ipAddressCanvas.setFont(&FreeSansBold18pt7b);
    ipAddressCanvas.setTextSize(1);
    ipAddressCanvas.setTextColor(lgfx::color565(255, 255, 255));

    String ipAddress = WiFi.localIP().toString();

    uint16_t width = ipAddressCanvas.textWidth(ipAddress.c_str());
    uint16_t height = ipAddressCanvas.fontHeight();

    int16_t x = ((ipAddressCanvas.width() - width) / 2) - 4;
    int16_t y = (ipAddressCanvas.height() - height);
    ipAddressCanvas.setCursor(x, y);
    ipAddressCanvas.print(ipAddress);

    ScopedMutex lock(spiMutex);
    ipAddressCanvas.pushSprite(0, tft.height() - spriteHeight);
}

void handleBuffer(tftMessage &msg)
{
    constexpr int16_t spriteWidth = 100;
    constexpr int16_t spriteHeight = 20;

    static LGFX_Sprite bufferStatus(&tft);
    if (!bufferStatus.getBuffer() && !bufferStatus.createSprite(spriteWidth, spriteHeight))
    {
        log_e("could not allocate canvas for buffer status. system halted");
        while (1)
            delay(100);
    }

    const int16_t FILLED_AREA = map_range(msg.value1, 0, msg.value2, 0, spriteWidth);
    bufferStatus.fillRect(0, 0, FILLED_AREA, spriteHeight, lgfx::color565(0, 255, 0));
    bufferStatus.fillRect(FILLED_AREA, 0, spriteWidth - FILLED_AREA, spriteHeight, lgfx::color565(255, 0, 0));

    ScopedMutex lock(spiMutex);
    bufferStatus.pushSprite(135, 50);
}

void handleLoading(tftMessage &msg)
{
    {
        ScopedMutex lock(spiMutex);
        loader.pushSprite(0, canvas.fontHeight());
    }
    if (playerTaskHandle)
        xTaskNotifyGive(playerTaskHandle);
}

void handleScroller()
{
    canvas.fillScreen(0);
    canvas.setCursor(canvas.width() - streamTitleOffset, canvas.height() - canvas.fontHeight());
    canvas.setFont(&FreeSansBold9pt7b);
    canvas.setTextSize(1);
    canvas.setTextWrap(false);
    canvas.print(streamTitle);
    {
        ScopedMutex lock(spiMutex);
        canvas.pushSprite(0, TOP_OF_SCROLLER);
    }
    streamTitleOffset = (streamTitleOffset < (canvas.width() + streamTitleWidth)) ? streamTitleOffset + 2 : 0;
    clearTitle = false;

    lastTitleShow = millis();
}

void handleClock()
{
    
    const time_t now = time(NULL);
    char currentTime[CLOCKSTR_LEN];
    strftime(currentTime, sizeof(currentTime), "%R", localtime(&now));

    if (strstr(previousTime, currentTime))
        return;

    static LGFX_Sprite clock(&tft);
    if (!clock.getBuffer() && !clock.createSprite(tft.width(), TOP_OF_SCROLLER))
    {
        log_e("could not allocate clock sprite. system halted.");
        while (1)
            delay(100);
    }

    clock.setFont(&FreeSansBold24pt7b);
    clock.setTextSize(2);
    clock.fillScreen(BACKGROUND_COLOR);
    const uint16_t width = clock.textWidth(currentTime);

    clock.setTextColor(TEXT_COLOR);
    clock.setCursor((clock.width() / 2) - (width / 2) - 5, 0);
    clock.print(currentTime);

    // make the characters a bit wider
    clock.setCursor((clock.width() / 2) - (width / 2) - 3, 0);
    clock.print(currentTime);
    {
        ScopedMutex lock(spiMutex);
        clock.pushSprite(0, 0);
    }
    snprintf(previousTime, sizeof(previousTime), "%s", currentTime);
}

void tftTask(void *parameter)
{
    // turn on the TFT / I2C power supply https://learn.adafruit.com/esp32-s3-reverse-tft-feather/pinouts#tft-display-3138945
    pinMode(TFT_I2C_POWER, OUTPUT);
    digitalWrite(TFT_I2C_POWER, HIGH);

    {
        ScopedMutex lock(spiMutex);
        tft.init();
        tft.setRotation(1);
        tft.fillScreen(BACKGROUND_COLOR);
        tft.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
        tft.setTextWrap(false);
        tft.setBrightness(35);
    }

    static constexpr int CANVAS_HEIGHT = 20;

    if (!canvas.getBuffer() && !canvas.createSprite(tft.width(), CANVAS_HEIGHT))
    {
        log_e("could not allocate canvas for station-name and scroller. system halted");
        while (1)
            delay(100);
    }

    if (!loader.getBuffer() && !loader.createSprite(tft.width(), TOP_OF_SCROLLER - CANVAS_HEIGHT))
    {
        log_e("could not allocate loader sprite. system halted.");
        while (1)
            delay(100);
    }

    static constexpr auto font = &DejaVu24;
    loader.fillScreen(BACKGROUND_COLOR);
    loader.setTextColor(lgfx::color565(255, 255, 255), BACKGROUND_COLOR);
    loader.drawCenterString("connecting...", loader.width() / 2, 2, font);

    constexpr int IDLE_RATE_HZ = 50;
    constexpr int IDLE_DELAY_MS = 1000 / IDLE_RATE_HZ;

    while (1)
    {
        static tftMessage msg = {};
        if (xQueueReceive(tftQueue, &msg, pdMS_TO_TICKS(IDLE_DELAY_MS)) == pdTRUE)
        {
            switch (msg.type)
            {
            case tftMessage::SYSTEM_MESSAGE:
                handleSysMess(msg);
                break;

            case tftMessage::PROGRESS_BAR:
                handleProgress(msg);
                break;

            case tftMessage::CLEAR_SCREEN:
                showClock = false;
                streamTitle[0] = 0;
                streamTitleOffset = 0;
                {
                    ScopedMutex lock(spiMutex);
                    tft.fillRect(0, canvas.height(), canvas.width(), TOP_OF_SCROLLER - canvas.height(), BACKGROUND_COLOR);
                }

                break;
            case tftMessage::SHOW_STATION:
                handleStation(msg);
                break;

            case tftMessage::SHOW_TITLE:
                handleTitle(msg);
                break;

            case tftMessage::SHOW_BITRATE:
                handleBitrate(msg);
                break;

            case tftMessage::SHOW_CODEC:
                handleCodec(msg);
                break;

            case tftMessage::SHOW_IPADDRESS:
                handleIp(msg);
                break;

            case tftMessage::SHOW_CLOCK:
                showClock = true;
                break;

            case tftMessage::BUFFER_STATUS:
                handleBuffer(msg);
                break;

            case tftMessage::SHOW_LOADING:
                handleLoading(msg);
                break;

            default:
                log_w("unhandled tft msg type");
            }
        }

        if (millis() - lastTitleShow > (IDLE_DELAY_MS - 1) && (streamTitle[0] || clearTitle))
            handleScroller();

        if (showClock)
            handleClock();
    }
}
