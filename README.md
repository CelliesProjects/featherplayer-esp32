# featherPlayer-esp32

This PlatformIO project is intended for a [Adafruit ESP32-S3 Reverse TFT Feather](https://www.adafruit.com/product/5691)
combined with a
[Adafruit Music Maker FeatherWing](https://www.adafruit.com/product/3357).

<img src="https://cdn-shop.adafruit.com/970x728/5691-04.jpg" alt="Adafruit ESP32-S3 Reverse TFT Feather" width="350"> <img src="https://cdn-shop.adafruit.com/970x728/3357-03.jpg" alt="Adafruit Music Maker FeatherWing" width="350">

## What is it?

A web-based esp32 music player for mp3/ogg/aac/aac+ webradio and files from a sdcard.<br>Sound output comes from a VS1053 mp3/aac/ogg/wav decoder board.

- Supports http, https (insecure mode)/chunked streams.<br>
- <strike>Plays local files from a mounted filesystem.&nbsp;</strike>Working on it.<br>
- Search for web radio stations on [radio-browser](https://www.radio-browser.info/).
- Save found radio stations to your favorites.

## Web based interface

This player has a web interface and is controlled with a browser on your phone, pc or tablet.<br>The interface is kept very simple, a playlist tab on the right and selectable source tabs on the left.

<img src="https://github.com/CelliesProjects/streamplayer-feather-develop/assets/24290108/0f9816be-bb35-4cd6-ac24-2fd2bb19561c" width="70%">

After 30 seconds of inactivity the file info overlay is shown.<br>Click on the overlay to hide it or use the info button on the bottom to toggle the overlay.

<img src="https://github.com/CelliesProjects/streamplayer-feather-develop/assets/24290108/0f3a9d8b-a698-450b-82ab-e4ac2b0569a3" width="70%">

On this overlay you can also save the current playing search result to the favorites tab by clicking the <img src="https://github.com/CelliesProjects/eStreamplayer32-vs1053-pio/assets/24290108/6e0d2706-45cd-4e5d-86ba-c194101afba7" style="background-color:white;"> icon.

## What you need

[Visual Studio Code](https://code.visualstudio.com/) with a working [PlatformIO](https://platformio.org/platformio-ide) plugin.

## Clone this project to your pc

- Open a terminal in the PIO project folder.
- Clone the project with the following command:

```
git clone https://github.com/CelliesProjects/featherplayer-esp32
```

- Use `File->Open folder` in Visual Studio and then browse to the folder where you cloned the project to.
- Press `Select` and the project will be opened and initialized, this will take some time while all assets are downloaded.

## Before compiling: add your WiFi credentials

Before compiling the project add the file `include/WiFicredentials.h` to supply your WiFi credentials:

```c++
#ifndef WIFI_SECRETS
#define WIFI_SECRETS

const char *SSID= "your wifi network name";
const char *PSK = "your wifi password";

#endif
```

That's it. Now you are ready to compile the project!

## About this project

- This project is a fork of [eStreamplayer32-vs1053-pio](https://github.com/CelliesProjects/eStreamplayer32-vs1053-pio)
- The SPI bus is shared by the tft, the vs1053 codec chip and the sd-card.
- SPI bus sharing between the cores and tasks is done through a [freeRTOS semaphore mutex](https://www.freertos.org/CreateMutex.html).

## GPIOs used


| SPI | GPIO |
| :----: | :----: |
| SCK |  36  |
| MISO |  37  |
| MOSI |  35  |


| SD SLOT | GPIO |
| :-------: | :----: |
|   CS   |  5  |


| VS1053 | GPIO |
| :------: | :----: |
|   CS   |  6  |
|  DCS  |  10  |
|  DREQ  |  9  |


| TFT | GPIO |
| :---: | :----: |
| CS |  42  |

# Resources used

https://docs.espressif.com/projects/esp-idf/en/v4.3/esp32/api-guides/freertos-smp.html

https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-guides/performance/speed.html#built-in-task-priorities

https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos_additions.html

https://github.com/khoih-prog/WiFiMulti_Generic/blob/main/examples/WiFiMulti/WiFiMulti.ino
