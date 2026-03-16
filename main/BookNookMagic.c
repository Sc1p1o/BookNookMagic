#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "raw_lcd";

#define PIN_NUM_SCLK      18
#define PIN_NUM_MOSI      23
#define PIN_NUM_CS        25
#define PIN_NUM_DC        26
#define PIN_NUM_RST       27

#define TFT_WIDTH         240
#define TFT_HEIGHT        240

static spi_device_handle_t lcd_spi;

static void lcd_reset(void)
{
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void lcd_send_cmd(uint8_t cmd)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    t.length = 8;
    t.tx_buffer = &cmd;

    gpio_set_level(PIN_NUM_DC, 0);
    ESP_ERROR_CHECK(spi_device_polling_transmit(lcd_spi, &t));
}

static void lcd_send_data(const void *data, int len_bytes)
{
    if (len_bytes <= 0) {
        return;
    }

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    t.length = len_bytes * 8;
    t.tx_buffer = data;

    gpio_set_level(PIN_NUM_DC, 1);
    ESP_ERROR_CHECK(spi_device_polling_transmit(lcd_spi, &t));
}

static void lcd_cmd_data(uint8_t cmd, const void *data, int len_bytes)
{
    lcd_send_cmd(cmd);
    lcd_send_data(data, len_bytes);
}

static void lcd_set_addr_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    data[0] = (x0 >> 8) & 0xFF;
    data[1] = x0 & 0xFF;
    data[2] = (x1 >> 8) & 0xFF;
    data[3] = x1 & 0xFF;
    lcd_cmd_data(0x2A, data, 4);

    data[0] = (y0 >> 8) & 0xFF;
    data[1] = y0 & 0xFF;
    data[2] = (y1 >> 8) & 0xFF;
    data[3] = y1 & 0xFF;
    lcd_cmd_data(0x2B, data, 4);

    lcd_send_cmd(0x2C);
}

static void lcd_fill_color(uint16_t color)
{
    static uint16_t line[TFT_WIDTH];
    uint8_t madctl = 0x08;
    uint8_t colmod = 0x55;

    for (int i = 0; i < TFT_WIDTH; i++) {
        line[i] = (uint16_t)((color << 8) | (color >> 8));
    }

    lcd_cmd_data(0x36, &madctl, 1);
    lcd_cmd_data(0x3A, &colmod, 1);

    lcd_set_addr_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

    for (int y = 0; y < TFT_HEIGHT; y++) {
        lcd_send_data(line, sizeof(line));
    }
}

static void lcd_init_minimal(void)
{
    uint8_t data;

    lcd_send_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_send_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));

    data = 0x55;
    lcd_cmd_data(0x3A, &data, 1);

    data = 0x08;
    lcd_cmd_data(0x36, &data, 1);

    lcd_send_cmd(0x21);

    data = 0x00;
    lcd_cmd_data(0x36, &data, 1);

    lcd_send_cmd(0x29);

    vTaskDelay(pdMS_TO_TICKS(20));
}

void app_main(void)
{
    ESP_LOGI(TAG, "raw spi lcd test start");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * 2
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,
        .mode = 3,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 1
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &devcfg, &lcd_spi));

    lcd_reset();
    ESP_LOGI(TAG, "reset done");

    lcd_init_minimal();
    ESP_LOGI(TAG, "minimal init done");

    while (1) {
        ESP_LOGI(TAG, "red");
        lcd_fill_color(0xF800);
        vTaskDelay(pdMS_TO_TICKS(1500));

        ESP_LOGI(TAG, "green");
        lcd_fill_color(0x07E0);
        vTaskDelay(pdMS_TO_TICKS(1500));

        ESP_LOGI(TAG, "blue");
        lcd_fill_color(0x001F);
        vTaskDelay(pdMS_TO_TICKS(1500));

        ESP_LOGI(TAG, "white");
        lcd_fill_color(0xFFFF);
        vTaskDelay(pdMS_TO_TICKS(1500));

        ESP_LOGI(TAG, "black");
        lcd_fill_color(0x0000);
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}



