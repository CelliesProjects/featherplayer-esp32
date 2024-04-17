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
    delay(10);

    // initialize the TFT
    const auto BACKGROUND_COLOR = ST77XX_YELLOW;

    xSemaphoreTake(spiMutex, portMAX_DELAY);
    static Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
    tft.init(135, 240, SPI_MODE0);
    tft.setRotation(1);
    tft.setFont(&FreeSansBold9pt7b);
    tft.fillScreen(BACKGROUND_COLOR);
    tft.setTextColor(ST77XX_BLACK, BACKGROUND_COLOR);
    tft.setTextSize(1);
    tft.setCursor(2, 14);
    tft.setTextWrap(false);
    tft.printf("tft started\n%s\n\n %s", (char*)PROGRAM_NAME, (char*)GIT_VERSION);
    xSemaphoreGive(spiMutex);

    // setup tft backlight pwm
    const auto LEDC_CHANNEL = 0;
    const auto LEDC_MAX_PWM_VALUE = (1 << SOC_LEDC_TIMER_BIT_WIDE_NUM) - 1;

    ledcSetup(LEDC_CHANNEL, 1220, SOC_LEDC_TIMER_BIT_WIDE_NUM);
    ledcAttachPin(TFT_BACKLITE, LEDC_CHANNEL);
    ledcWrite(LEDC_CHANNEL, LEDC_MAX_PWM_VALUE >> (SOC_LEDC_TIMER_BIT_WIDE_NUM / 3));

    // setup stuff for the main loop
    static GFXcanvas16 canvas(tft.width(), 20);
    static int16_t strX, strY;
    static uint16_t strWidth, strHeight;

    static char streamTitle[264];
    static int16_t streamTitleOffset = 0;

    while (1)
    {
        static tftMessage msg = {};
        if (xQueueReceive(tftQueue, &msg, pdTICKS_TO_MS(25)) == pdTRUE)
        {
            switch (msg.action)
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
                const int HEIGHT_IN_PIXELS = 20;
                const int HEIGHT_OFFSET = 64;
                const int16_t FILLED_AREA = map_range(msg.value1, 0, msg.value2, 0, tft.width() + 1);
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.fillRect(0, HEIGHT_OFFSET, FILLED_AREA, HEIGHT_IN_PIXELS, ST77XX_BLUE);
                tft.fillRect(FILLED_AREA, HEIGHT_OFFSET, tft.width() - FILLED_AREA, HEIGHT_IN_PIXELS, ST77XX_WHITE);
                xSemaphoreGive(spiMutex);
                break;
            }
            case tftMessage::CLEAR_SCREEN:
                streamTitle[0] = 0;
                streamTitleOffset = 0;
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.fillScreen(BACKGROUND_COLOR);
                xSemaphoreGive(spiMutex);
                break;
            case tftMessage::SHOW_STATION:
                tft.setTextSize(1);
                tft.setTextColor(ST77XX_BLACK);
                tft.setFont(&FreeSansBold9pt7b);
                tft.setCursor(5, 34);
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.print(msg.str);
                xSemaphoreGive(spiMutex);
                break;
            case tftMessage::SHOW_TITLE:
                if (msg.str[0] == 0)
                    break;
                streamTitleOffset = 0;
                snprintf(streamTitle, sizeof(streamTitle), "%s", msg.str);
                tft.setTextSize(1);
                tft.setFont(&FreeSansBold9pt7b);
                tft.getTextBounds(streamTitle, 0, 0, &strX, &strY, &strWidth, &strHeight);
                break;
            case tftMessage::SHOW_BITRATE:
                tft.setTextSize(1);
                tft.setTextColor(ST77XX_BLACK);
                tft.setFont(&FreeSansBold9pt7b);
                tft.setCursor(5, 56);
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.print(msg.value1);
                tft.print(" kbps");
                xSemaphoreGive(spiMutex);
                break;
            case tftMessage::SHOW_CODEC:
                tft.setTextSize(1);
                tft.setTextColor(ST77XX_BLACK);
                tft.setFont(&FreeSansBold9pt7b);
                tft.setCursor(85, 56);
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.print(msg.str);
                xSemaphoreGive(spiMutex);
                break;
            case tftMessage::SHOW_IPADDRESS:
            {
                tft.setFont(&FreeSansBold18pt7b);
                tft.setTextSize(1);
                tft.setTextColor(ST77XX_BLACK);
                static int16_t xpos;
                static int16_t ypos;
                static uint16_t height;
                static uint16_t width;
                tft.getTextBounds(WiFi.localIP().toString().c_str(), 0, 0, &xpos, &ypos, &width, &height);
                tft.setCursor((tft.width() / 2) - (width / 2), tft.height() - (height / 2));
                xSemaphoreTake(spiMutex, portMAX_DELAY);
                tft.print(WiFi.localIP().toString().c_str());
                xSemaphoreGive(spiMutex);
                break;
            }
            default:
                log_w("unhandled tft msg type");
            }
        }

        if (streamTitle[0])
        {
            const auto Y_POSITION = 64;

            canvas.fillScreen(0);
            canvas.setCursor(canvas.width() - streamTitleOffset, canvas.height() - 6);
            canvas.setFont(&FreeSansBold9pt7b);
            canvas.setTextSize(1);
            canvas.setTextWrap(false);
            canvas.print(streamTitle);

            xSemaphoreTake(spiMutex, portMAX_DELAY);
            tft.drawRGBBitmap(0, Y_POSITION, canvas.getBuffer(), canvas.width(), canvas.height());
            xSemaphoreGive(spiMutex);

            streamTitleOffset = (streamTitleOffset < (canvas.width() + strWidth)) ? streamTitleOffset + 2 : 0;
        }
    }
}
