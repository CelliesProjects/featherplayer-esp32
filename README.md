# featherPlayer-esp32

This software is intended for a [Adafruit ESP32-S3 Reverse TFT Feather](https://www.adafruit.com/product/5691)
combined with a 
[Adafruit Music Maker FeatherWing](https://www.adafruit.com/product/3357)

<img src="https://cdn-shop.adafruit.com/970x728/5691-04.jpg" alt="Adafruit ESP32-S3 Reverse TFT Feather" width="350"> <img src="https://cdn-shop.adafruit.com/970x728/3357-03.jpg" alt="Adafruit Music Maker FeatherWing" width="350">

## what is it

A web-based esp32 music player for mp3/ogg/aac/aac+ webradio and files from a sdcard.

Sound output comes from a separate VS1053 mp3/aac/ogg/wav decoder breakout board.

-  Supports http, https (insecure mode)/chunked streams.<br>
-  <strike>Plays local files from a mounted filesystem.&nbsp;</strike>Working on it.<br>
-  Search for web radio stations on radio-browser and save the results to your favorites.

## web based interface

The music player is controlled with a browser on your phone, pc or tablet.

The interface is kept very simple, a playlist tab on the right and selectable source tabs on the left.<br>
After 30 seconds of inactivity the file info overlay is shown.<br>
Click on the overlay to hide it or use the info button on the bottom to toggle the overlay.

On this overlay you can also save the current playing search result to the favorites tab with this <img src="https://github.com/CelliesProjects/eStreamplayer32-vs1053-pio/assets/24290108/6e0d2706-45cd-4e5d-86ba-c194101afba7"> icon.

## before compiling: add your WiFi credentials

Before compiling the project you need to edit the file `include/WiFicredentials.h` to supply your WiFi credentials:
```c++
#ifndef WIFI_SECRETS
#define WIFI_SECRETS

const char *SSID= "your wifi network name";
const char *PSK = "your wifi password";

#endif
```

## About

-  This project is a fork of [eStreamplayer32-vs1053-pio](https://github.com/CelliesProjects/eStreamplayer32-vs1053-pio)
-  The SPI bus is shared by the tft, the vs1053 codec chip and the sd-card.
-  SPI bus sharing between the cores and tasks is done through a [freeRTOS semaphore mutex](https://www.freertos.org/CreateMutex.html).
## GPIOs used


| SPI  | GPIO |
| :---: |  :---: |
| SCK | 36 |
| MISO | 37 |
| MOSI | 35 |

| SD SLOT  | GPIO |
| :---: |  :---: |
| CS | 5 |

| VS1053  | GPIO |
| :---: |  :---: |
| CS | 6 |
| DCS | 10 |
| DREQ | 9 |

| TFT  | GPIO |
| :---: |  :---: |
| CS | 42 |

# Resources used
https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-guides/freertos-smp.html

https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-guides/performance/speed.html#built-in-task-priorities

https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos_additions.html

https://github.com/khoih-prog/WiFiMulti_Generic/blob/main/examples/WiFiMulti/WiFiMulti.ino
