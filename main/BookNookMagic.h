#pragma once

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_err.h"

#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7735.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAILED_BIT    BIT1

#define MAX_IMAGE_URLS     16
#define MAX_IMAGE_URL_LEN  256

#define PIN_NUM_SCLK    18
#define PIN_NUM_MOSI    23
#define PIN_NUM_CS      5
#define PIN_NUM_DC      16
#define PIN_NUM_RST     17
#define PIN_NUM_BCKL    4

#define TFT_H_RES       128
#define TFT_V_RES       160
#define TFT_FRAME_SIZE  (TFT_H_RES * TFT_V_RES * 2)

#define IMAGE_DOWNLOAD_MAX_SIZE (64 * 1024)

extern bool g_server_online;
extern const char *TAG;
extern char http_response_buffer[4096];
extern int http_response_length;
extern EventGroupHandle_t wifi_event_group;

void uart_console_init(void);
void wifi_init_sta(void);
void wifi_connect_from_console(void);
void test_server_health(void);
void trim_newline(char *str);
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);
int uart_read_line(char *buffer, size_t max_len, bool echo, bool mask_input);

bool fetch_image_list(char urls[][MAX_IMAGE_URL_LEN], int *count);

void tft_init(void);
void tft_fill_screen(uint16_t color);
void tft_set_rotation(uint8_t rotation);
void slideshow_task(void *arg);
esp_err_t http_event_handler_images(esp_http_client_event_t *evt);

void fill_solid_color(esp_lcd_panel_handle_t panel, uint16_t color_rgb565);
void init_tft_panel(void);

bool download_image_jpg(const char *url, uint8_t **jpg_data, size_t *jpg_size);
bool display_jpg_from_memory(const uint8_t *jpg_data, size_t jpg_size);
bool display_jpg_from_url(const char *url);















