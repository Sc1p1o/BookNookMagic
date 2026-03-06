#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// ST7735 Treiber aus managed_components:
#include "esp_lcd_st7735.h"

#include "Flame1.h"


static const char *TAG = "tft";

#define PIN_NUM_SCLK    18   // SCK
#define PIN_NUM_MOSI    23   // SDA (MOSI)
#define PIN_NUM_CS      5    // CS
#define PIN_NUM_DC      16   // A0 (D/C)
#define PIN_NUM_RST     17   // RESET
#define PIN_NUM_BCKL    4    // LED (Backlight) - optional, -1 wenn fest an 3V3

#define TFT_H_RES       128
#define TFT_V_RES       160


static void fill_solid_color(esp_lcd_panel_handle_t panel, uint16_t color_rgb565)
{
    static uint16_t line[TFT_H_RES];
    for (int x = 0; x < TFT_H_RES; x++) {
        line[x] = color_rgb565;
    }
    for (int y = 0; y < TFT_V_RES; y++) {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, TFT_H_RES, y + 1, line));
    }
}

void app_main(void) {
    if (PIN_NUM_BCKL >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << PIN_NUM_BCKL,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        gpio_set_level(PIN_NUM_BCKL, 1);
    }

    // Optional: du kannst auch das Makro aus esp_lcd_st7735.h nehmen:
    // spi_bus_config_t buscfg = st7735_PANEL_BUS_SPI_CONFIG(PIN_NUM_SCLK, PIN_NUM_MOSI, TFT_H_RES * TFT_V_RES * 2);
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_H_RES * TFT_V_RES * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;

    // Optional: Makro aus esp_lcd_st7735.h:
    // esp_lcd_panel_io_spi_config_t io_config = st7735_PANEL_IO_SPI_CONFIG(PIN_NUM_CS, PIN_NUM_DC, NULL, NULL);
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = 26 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;

    // Vendor config: NULL = Default-Init aus der Komponente
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
        .vendor_config = NULL,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7735(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    // Diese drei Schalter sind bei ST7735-Modulen oft nötig (je nach “Tab”-Variante).
    // Wenn du nachher falsche Farben/Orientierung hast: einzeln togglen/testen.
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));

    ESP_LOGI(TAG, "Test colors (wenn das klappt, klappt auch die Flamme)");
    fill_solid_color(panel_handle, 0x2F0F); // rot
    vTaskDelay(pdMS_TO_TICKS(500));
    fill_solid_color(panel_handle, 0xFFF0); // grün
    vTaskDelay(pdMS_TO_TICKS(500));
    fill_solid_color(panel_handle, 0xF0FF); // blau
    vTaskDelay(pdMS_TO_TICKS(500));
    fill_solid_color(panel_handle, 0x0000); // schwarz

    ESP_LOGI(TAG, "Draw flame");

    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 127, 142, Flame1));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
