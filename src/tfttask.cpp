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

    static const auto BACKGROUND_COLOR = 0xa0e0; // 8 bit value = #a61d04
    static const auto TEXT_COLOR = 0xf79b;       // 8 bit value = #f5f4e2 yellowish

    static Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

    {
        ScopedMutex lock(spiMutex);
        tft.init(135, 240, SPI_MODE0);
        tft.setRotation(1);
        tft.fillScreen(BACKGROUND_COLOR);
        tft.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
        tft.setTextWrap(false);
    }

    const auto LEDC_MAX_PWM_VALUE = (1 << SOC_LEDC_TIMER_BIT_WIDTH) - 1;

    ledcAttachChannel(TFT_BACKLITE, 1220, SOC_LEDC_TIMER_BIT_WIDTH, 0);
    ledcWrite(TFT_BACKLITE, LEDC_MAX_PWM_VALUE / 10);

    static GFXcanvas16 canvas(tft.width(), 20);
    static int16_t strX, strY;
    static uint16_t strWidth, strHeight;

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
                canvas.fillScreen(ST77XX_RED);
                canvas.setTextColor(ST77XX_WHITE);
                canvas.setFont(&FreeSansBold9pt7b);
                canvas.setTextSize(1);
                canvas.setCursor(4, canvas.height() - 6);
                canvas.print(msg.str);
                {
                    ScopedMutex lock(spiMutex);
                    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height());
                }
                break;

            case tftMessage::PROGRESS_BAR:
            {
                const int HEIGHT_IN_PIXELS = 15;
                const int HEIGHT_OFFSET = 28;
                const int16_t FILLED_AREA = map_range(msg.value1, 0, msg.value2, 0, tft.width() + 1);
                {
                    ScopedMutex lock(spiMutex);
                    tft.fillRect(0, HEIGHT_OFFSET, FILLED_AREA, HEIGHT_IN_PIXELS, ST77XX_BLUE);
                    tft.fillRect(FILLED_AREA, HEIGHT_OFFSET, tft.width() - FILLED_AREA, HEIGHT_IN_PIXELS, ST77XX_WHITE);
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
                canvas.setCursor(4, canvas.height() - 6);
                canvas.print(msg.str);
                {
                    ScopedMutex lock(spiMutex);
                    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height());
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
                tft.getTextBounds(streamTitle, 0, 0, &strX, &strY, &strWidth, &strHeight);
                break;
            case tftMessage::SHOW_CODEC:
                tft.setTextSize(1);
                tft.setTextColor(TEXT_COLOR);
                tft.setFont(&FreeSansBold9pt7b);
                tft.setCursor(5, 66);
                {
                    ScopedMutex lock(spiMutex);
                    tft.print(msg.str);
                }
                tft.setFont();
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
                GFXcanvas16 ipAddressCanvas(tft.width(), spriteHeight);

                if (!ipAddressCanvas.getBuffer())
                {
                    log_e("could not allocate ipaddress canvas");
                    break;
                }
                ipAddressCanvas.fillScreen(BACKGROUND_COLOR);
                ipAddressCanvas.setFont(&FreeSansBold18pt7b);
                ipAddressCanvas.setTextSize(1);
                ipAddressCanvas.setTextColor(ST77XX_WHITE);

                String ipAddress = WiFi.localIP().toString();
                int16_t xPos, yPos;
                uint16_t width, height;

                ipAddressCanvas.getTextBounds(ipAddress.c_str(), 0, 0, &xPos, &yPos, &width, &height);

                int16_t x = ((ipAddressCanvas.width() - width) / 2) - 4;
                int16_t y = (ipAddressCanvas.height() - 8);
                ipAddressCanvas.setCursor(x, y);
                ipAddressCanvas.print(ipAddress);
                {
                    ScopedMutex lock(spiMutex);
                    tft.drawRGBBitmap(0, tft.height() - spriteHeight, ipAddressCanvas.getBuffer(), tft.width(), spriteHeight);
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
                    tft.fillRect(X_OFFSET, HEIGHT_OFFSET, FILLED_AREA, BAR_HEIGHT, ST77XX_GREEN);
                    tft.fillRect(X_OFFSET + FILLED_AREA, HEIGHT_OFFSET, BAR_WIDTH - FILLED_AREA, BAR_HEIGHT, ST77XX_RED);
                }
                break;
            }

            default:
                log_w("unhandled tft msg type");
            }
        }

        static time_t lastClockUpdate = 0;

        if (showClock && lastClockUpdate != time(NULL))
        {
            GFXcanvas16 clock(tft.width(), TOP_OF_SCROLLER);

            clock.setFont(&FreeSansBold24pt7b);
            clock.setTextSize(2);
            clock.fillScreen(BACKGROUND_COLOR);

            time_t t;
            time(&t);
            struct tm *timeinfo = localtime(&t);
            char buff[12];
            int16_t xpos;
            int16_t ypos;
            uint16_t height;
            uint16_t width;
            strftime(buff, 12, "%R", timeinfo);
            clock.getTextBounds(buff, 0, 0, &xpos, &ypos, &width, &height);
            clock.setTextColor(TEXT_COLOR);
            clock.setCursor((clock.width() / 2) - (width / 2) - 5, TOP_OF_SCROLLER - 6);
            clock.print(buff);

            // make the letters a bit wider
            clock.setCursor((clock.width() / 2) - (width / 2) - 3, TOP_OF_SCROLLER - 6);
            clock.print(buff);
            {
                ScopedMutex lock(spiMutex);
                tft.drawRGBBitmap(0, 0, clock.getBuffer(), clock.width(), clock.height());
            }
            lastClockUpdate = time(NULL);
        }

        if (streamTitle[0] || clearTitle)
        {
            canvas.fillScreen(0);
            canvas.setCursor(canvas.width() - streamTitleOffset, canvas.height() - 6);
            canvas.setFont(&FreeSansBold9pt7b);
            canvas.setTextSize(1);
            canvas.setTextWrap(false);
            canvas.print(streamTitle);
            {
                ScopedMutex lock(spiMutex);
                tft.drawRGBBitmap(0, TOP_OF_SCROLLER, canvas.getBuffer(), canvas.width(), canvas.height());
            }
            streamTitleOffset = (streamTitleOffset < (canvas.width() + strWidth)) ? streamTitleOffset + 2 : 0;
            clearTitle = false;
        }
    }
}
