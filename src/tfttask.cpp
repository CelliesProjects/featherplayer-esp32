#include "tfttask.h"

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
    // turn on the TFT / I2C power supply
    pinMode(TFT_I2C_POWER, OUTPUT);
    digitalWrite(TFT_I2C_POWER, HIGH);
    delay(5);

    static const auto BACKGROUND_COLOR = 0x18e3; // 8 bit value = #1c1c1c dark grey
    static const auto TEXT_COLOR = 0xf79b;       // 8 bit value = #f5f4e2 yellowish

    // initialize the TFT
    xSemaphoreTake(spiMutex, portMAX_DELAY);
    static Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
    tft.init(135, 240, SPI_MODE0);
    tft.setRotation(1);
    tft.fillScreen(BACKGROUND_COLOR);
    tft.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
    tft.setTextWrap(false);
    xSemaphoreGive(spiMutex);

    // setup tft backlight pwm
    const auto LEDC_CHANNEL = 0;
    const auto LEDC_MAX_PWM_VALUE = (1 << SOC_LEDC_TIMER_BIT_WIDE_NUM) - 1;

    ledcSetup(LEDC_CHANNEL, 1220, SOC_LEDC_TIMER_BIT_WIDE_NUM);
    ledcAttachPin(TFT_BACKLITE, LEDC_CHANNEL);
    ledcWrite(LEDC_CHANNEL, LEDC_MAX_PWM_VALUE / 10);

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
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height());
                xSemaphoreGive(spiMutex);
                break;

            case tftMessage::PROGRESS_BAR:
            {
                const int HEIGHT_IN_PIXELS = 15;
                const int HEIGHT_OFFSET = 28;
                const int16_t FILLED_AREA = map_range(msg.value1, 0, msg.value2, 0, tft.width() + 1);
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.fillRect(0, HEIGHT_OFFSET, FILLED_AREA, HEIGHT_IN_PIXELS, ST77XX_BLUE);
                tft.fillRect(FILLED_AREA, HEIGHT_OFFSET, tft.width() - FILLED_AREA, HEIGHT_IN_PIXELS, ST77XX_WHITE);
                xSemaphoreGive(spiMutex);
                break;
            }
            case tftMessage::CLEAR_SCREEN:
                showClock = false;
                streamTitle[0] = 0;
                streamTitleOffset = 0;
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.fillRect(0, 0, tft.width(), TOP_OF_SCROLLER, BACKGROUND_COLOR);
                xSemaphoreGive(spiMutex);
                break;
            case tftMessage::SHOW_STATION:
                tft.setTextSize(1);
                tft.setTextColor(TEXT_COLOR);
                tft.setFont(&FreeSansBold9pt7b);
                tft.setCursor(5, 20);
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.print(msg.str);
                xSemaphoreGive(spiMutex);
                break;
            case tftMessage::SHOW_TITLE:
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
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.print(msg.str);
                xSemaphoreGive(spiMutex);

                tft.setFont();
                tft.setTextSize(1);
                tft.setCursor(152, 53);
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.print("buffer status");
                tft.drawRect(148, 50, 85, 24, TEXT_COLOR);
                xSemaphoreGive(spiMutex);
                break;
            case tftMessage::SHOW_IPADDRESS:
            {
                tft.setFont(&FreeSansBold18pt7b);
                tft.setTextSize(1);
                tft.setTextColor(TEXT_COLOR);
                int16_t xpos;
                int16_t ypos;
                uint16_t height;
                uint16_t width;
                tft.getTextBounds(WiFi.localIP().toString().c_str(), 0, 0, &xpos, &ypos, &width, &height);
                tft.setCursor((tft.width() / 2) - (width / 2), tft.height() - (height / 2) + 4);
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.print(WiFi.localIP().toString().c_str());
                xSemaphoreGive(spiMutex);
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
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.fillRect(X_OFFSET, HEIGHT_OFFSET, FILLED_AREA, BAR_HEIGHT, ST77XX_GREEN);
                tft.fillRect(X_OFFSET + FILLED_AREA, HEIGHT_OFFSET, BAR_WIDTH - FILLED_AREA, BAR_HEIGHT, ST77XX_RED);
                xSemaphoreGive(spiMutex);
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
            strftime(buff, sizeof(buff), "%R", timeinfo);
            clock.getTextBounds(buff, 0, 0, &xpos, &ypos, &width, &height);
            clock.setTextColor(TEXT_COLOR);
            clock.setCursor((clock.width() / 2) - (width / 2) - 5, TOP_OF_SCROLLER - 6);
            clock.print(buff);

            clock.setTextColor(TEXT_COLOR);
            clock.setCursor((clock.width() / 2) - (width / 2) - 3, TOP_OF_SCROLLER - 6);
            clock.print(buff);

            xSemaphoreTake(spiMutex, portMAX_DELAY);
            tft.drawRGBBitmap(0, 0, clock.getBuffer(), clock.width(), clock.height());
            xSemaphoreGive(spiMutex);
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

            xSemaphoreTake(spiMutex, portMAX_DELAY);
            tft.drawRGBBitmap(0, TOP_OF_SCROLLER, canvas.getBuffer(), canvas.width(), canvas.height());
            xSemaphoreGive(spiMutex);

            streamTitleOffset = (streamTitleOffset < (canvas.width() + strWidth)) ? streamTitleOffset + 2 : 0;
            clearTitle = false;
        }
    }
}
