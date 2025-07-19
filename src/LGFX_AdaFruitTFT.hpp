#pragma once
#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
public:
    LGFX()
    {
        auto cfg = _panel.config();

        cfg.pin_cs = TFT_CS;
        cfg.pin_rst = TFT_RST;
        cfg.pin_busy = -1;

        cfg.memory_width = 240;
        cfg.memory_height = 240;
        cfg.panel_width = 135;
        cfg.panel_height = 240;
        cfg.offset_x = 52;
        cfg.offset_y = 40;
        cfg.offset_rotation = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits = 1;
        cfg.readable = false;
        cfg.invert = true;
        cfg.rgb_order = false;
        cfg.dlen_16bit = false;
        cfg.bus_shared = true;

        _panel.config(cfg);

        auto buscfg = _bus_instance.config();
        buscfg.spi_host = SPI2_HOST;
        buscfg.spi_mode = 0;
        buscfg.freq_write = 40000000;
        buscfg.freq_read = 16000000;
        buscfg.spi_3wire = false;
        buscfg.use_lock = true;
        buscfg.dma_channel = 1;
        buscfg.pin_sclk = SCK;
        buscfg.pin_mosi = MOSI;
        buscfg.pin_miso = MISO;
        buscfg.pin_dc = TFT_DC;

        _bus_instance.config(buscfg);
        _panel.setBus(&_bus_instance);

        setPanel(&_panel);
    }

private:
    lgfx::Bus_SPI _bus_instance;
    lgfx::Panel_ST7789 _panel;
};