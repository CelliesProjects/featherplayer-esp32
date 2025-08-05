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

void tftTask(void *parameter)
{
    // turn on the TFT / I2C power supply https://learn.adafruit.com/esp32-s3-reverse-tft-feather/pinouts#tft-display-3138945
    pinMode(TFT_I2C_POWER, OUTPUT);
    digitalWrite(TFT_I2C_POWER, HIGH);
    delay(5);

    static const auto BACKGROUND_COLOR = 0xa0e0; // RGB888 value = #a61d04
    static const auto TEXT_COLOR = 0xf79b;       // RGB888 value = #f5f4e2 yellowish

    static LGFX tft;

    {
        ScopedMutex lock(spiMutex);
        tft.init();
        tft.setRotation(1);
        tft.fillScreen(BACKGROUND_COLOR);
        tft.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
        tft.setTextWrap(false);
        tft.setBrightness(35);
    }

    static LGFX_Sprite canvas(&tft);
    if (!canvas.getBuffer() && !canvas.createSprite(tft.width(), 20))
    {
        log_e("could not allocate canvas for station-name and scroller. system halted");
        while (1)
            delay(100);
    }

    static uint16_t strWidth;

    static char streamTitle[264];
    static int16_t streamTitleOffset = 0;

    static const auto TOP_OF_SCROLLER = 76;
    static bool showClock = false;

    while (1)
    {
        bool clearTitle = false;
        static tftMessage msg = {};
        if (xQueueReceive(tftQueue, &msg, pdTICKS_TO_MS(25)) == pdTRUE)
        {
            switch (msg.type)
            {
            case tftMessage::SYSTEM_MESSAGE:
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
                break;

            case tftMessage::PROGRESS_BAR:
            {
                const int HEIGHT_IN_PIXELS = 15;
                const int HEIGHT_OFFSET = 28;
                const int16_t FILLED_AREA = map_range(msg.value1, 0, msg.value2, 0, tft.width() + 1);
                {
                    ScopedMutex lock(spiMutex);
                    tft.fillRect(0, HEIGHT_OFFSET, FILLED_AREA, HEIGHT_IN_PIXELS, lgfx::color565(0, 0, 255));
                    tft.fillRect(FILLED_AREA, HEIGHT_OFFSET, tft.width() - FILLED_AREA, HEIGHT_IN_PIXELS, lgfx::color565(255, 255, 255));
                }
                break;
            }
            case tftMessage::CLEAR_SCREEN:
                showClock = false;
                streamTitle[0] = 0;
                streamTitleOffset = 0;
                {
                    ScopedMutex lock(spiMutex);
                    tft.fillRect(0, canvas.height(), tft.width(), TOP_OF_SCROLLER - canvas.height(), BACKGROUND_COLOR);
                }

                break;
            case tftMessage::SHOW_STATION:
                canvas.fillScreen(0);
                canvas.setFont(&FreeSansBold9pt7b);
                canvas.setTextSize(1);
                canvas.setCursor(4, canvas.height() - canvas.fontHeight());
                canvas.print(msg.str);
                {
                    ScopedMutex lock(spiMutex);
                    canvas.pushSprite(0, 0);
                }

                break;
            case tftMessage::SHOW_TITLE:
                if (!strcmp(streamTitle, msg.str))
                    break;

                if (msg.str[0] == 0)
                {
                    clearTitle = true;
                    streamTitle[0] = 0;
                    break;
                }
                streamTitleOffset = 0;
                snprintf(streamTitle, sizeof(streamTitle), "%s", msg.str);
                tft.setTextSize(1);
                tft.setFont(&FreeSansBold9pt7b);
                strWidth = tft.textWidth(streamTitle);
                break;
            case tftMessage::SHOW_CODEC:
                tft.setTextSize(1);
                tft.setTextColor(TEXT_COLOR);
                tft.setFont(&FreeSansBold9pt7b);
                tft.setCursor(5, 55);
                {
                    ScopedMutex lock(spiMutex);
                    tft.print(msg.str);
                }
                tft.setFont(0);
                tft.setTextSize(1);
                tft.setCursor(152, 53);
                {
                    ScopedMutex lock(spiMutex);
                    tft.print("buffer status");
                    tft.drawRect(148, 50, 85, 24, TEXT_COLOR);
                }
                break;
            case tftMessage::SHOW_IPADDRESS:
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
                {
                    ScopedMutex lock(spiMutex);
                    ipAddressCanvas.pushSprite(0, tft.height() - spriteHeight);
                }
                break;
            }

            case tftMessage::SHOW_CLOCK:
                showClock = true;
                break;

            case tftMessage::BUFFER_STATUS:
            {
                const int X_OFFSET = 150;
                const int HEIGHT_OFFSET = 63;
                const int BAR_WIDTH = 80;
                const int BAR_HEIGHT = 7;
                const int16_t FILLED_AREA = map_range(msg.value1, 0, msg.value2, 0, BAR_WIDTH);
                {
                    ScopedMutex lock(spiMutex);
                    tft.fillRect(X_OFFSET, HEIGHT_OFFSET, FILLED_AREA, BAR_HEIGHT, lgfx::color565(0, 255, 0));
                    tft.fillRect(X_OFFSET + FILLED_AREA, HEIGHT_OFFSET, BAR_WIDTH - FILLED_AREA, BAR_HEIGHT, lgfx::color565(255, 0, 0));
                }
                break;
            }

            default:
                log_w("unhandled tft msg type");
            }
        }

        time_t t;
        time(&t);
        struct tm *timeinfo = localtime(&t);
        char buff[CLOCKSTR_LEN];
        strftime(buff, sizeof(buff), "%R", timeinfo);

        if (showClock && !strstr(previousTime, buff))
        {
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
            const uint16_t width = clock.textWidth(buff);

            clock.setTextColor(TEXT_COLOR);
            clock.setCursor((clock.width() / 2) - (width / 2) - 5, 0);
            clock.print(buff);

            // make the characters a bit wider
            clock.setCursor((clock.width() / 2) - (width / 2) - 3, 0);
            clock.print(buff);
            {
                ScopedMutex lock(spiMutex);
                clock.pushSprite(0, 0);
            }
            snprintf(previousTime, sizeof(previousTime), "%s", buff);
        }

        if (streamTitle[0] || clearTitle)
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
            streamTitleOffset = (streamTitleOffset < (canvas.width() + strWidth)) ? streamTitleOffset + 2 : 0;
            clearTitle = false;
        }
    }
}
