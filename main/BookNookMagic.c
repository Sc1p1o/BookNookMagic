#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"

static const char *TAG = "tft";

#define PIN_NUM_SCLK      18
#define PIN_NUM_MOSI      23

#define TFT3_PIN_NUM_CS   25
#define TFT3_PIN_NUM_DC   26
#define TFT3_PIN_NUM_RST  27

#define TFT_H_RES         240
#define TFT_V_RES         240

static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle = NULL;

static void fill_color(uint16_t color)
{
    static uint16_t line[TFT_H_RES];

    for (int x = 0; x < TFT_H_RES; x++) {
        line[x] = color;
    }

    for (int y = 0; y < TFT_V_RES; y++) {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, TFT_H_RES, y + 1, line));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "start");

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_NUM_SCLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_H_RES * TFT_V_RES * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "spi ok");

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = TFT3_PIN_NUM_DC,
        .cs_gpio_num = TFT3_PIN_NUM_CS,
        .pclk_hz = 5 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_config, &io_handle));
    ESP_LOGI(TAG, "panel io ok");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = TFT3_PIN_NUM_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
        .vendor_config = NULL,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle));
    ESP_LOGI(TAG, "panel create ok");

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "fill white");
    fill_color(0x0000);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}