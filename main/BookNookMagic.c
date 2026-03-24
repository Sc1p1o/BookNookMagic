#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "sdkconfig.h"



#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

// ST7735 Treiber aus managed_components:
#include "esp_lcd_st7735.h"

#include "Flame1.h"
#include "FlameUnified1.h"
#include "FlameUnified2.h"
#include "FlameUnified3.h"
#include "FlameUnified4.h"
#include "FlameUnified6.h"
#include "FlameUnified7.h"
#include "FlameUnified8.h"
#include "FlameUnified9.h"

#define PIN_NUM_SCLK    18   // SCK
#define PIN_NUM_MOSI    23   // SDA (MOSI)
#define PIN_NUM_CS      5    // CS
#define PIN_NUM_DC      16   // A0 (D/C)
#define PIN_NUM_RST     17   // RESET
#define PIN_NUM_BCKL    4    // LED (Backlight) - optional, -1 wenn fest an 3V3

#define TFT_H_RES       128
#define TFT_V_RES       160

#define FRAME_TIME_MS   200

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAXIMUM_RETRY      5

static const char *TAG = "wifi station";
static EventGroupHandle_t wifi_event_group;
static int s_retry_num = 0;

static const char *TAG = "tft";



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

void animaltion_task(void *pvParameters)
{
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
        .pclk_hz = 10 * 1000 * 1000,
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

    fill_solid_color(panel_handle, 0x0000); // schwarz


    while (1) {
        vTaskDelay(pdMS_TO_TICKS(FRAME_TIME_MS));
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 115, 160, FlameUnified1));

        vTaskDelay(pdMS_TO_TICKS(FRAME_TIME_MS));
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 115, 160, FlameUnified2));

        vTaskDelay(pdMS_TO_TICKS(FRAME_TIME_MS));
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 115, 160, FlameUnified3));

        vTaskDelay(pdMS_TO_TICKS(FRAME_TIME_MS));
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 115, 160, FlameUnified4));

        vTaskDelay(pdMS_TO_TICKS(FRAME_TIME_MS));
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 115, 160, FlameUnified6));

        vTaskDelay(pdMS_TO_TICKS(FRAME_TIME_MS));
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 115, 160, FlameUnified7));

        vTaskDelay(pdMS_TO_TICKS(FRAME_TIME_MS));
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 115, 160, FlameUnified8));

        vTaskDelay(pdMS_TO_TICKS(FRAME_TIME_MS));
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, 115, 160, FlameUnified9));
    }
}

void presence_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Presence Task started");
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Verbindungsversuch erneut: %d/%d", s_retry_num, MAXIMUM_RETRY);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "Verbindung zum WLAN getrennt");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {0};
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    strncpy((char *)wifi_config.sta.ssid,
            CONFIG_BOOKNOOK_WIFI_SSID,
            sizeof(wifi_config.sta.ssid) - 1);

    strncpy((char *)wifi_config.sta.password,
            CONFIG_BOOKNOOK_WIFI_PASSWORD,
            sizeof(wifi_config.sta.password) - 1);

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_LOGI(TAG,
             "Verbinde mit SSID:%s Passwort:%s",
             CONFIG_BOOKNOOK_WIFI_SSID,
             CONFIG_BOOKNOOK_WIFI_PASSWORD);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG,
                 "connected to ap SSID:%s password:%s",
                 CONFIG_BOOKNOOK_WIFI_SSID,
                 CONFIG_BOOKNOOK_WIFI_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG,
                 "Verbindung fehlgeschlagen zu SSID:%s Passwort:%s",
                 CONFIG_BOOKNOOK_WIFI_SSID,
                 CONFIG_BOOKNOOK_WIFI_PASSWORD);
    } else {
        ESP_LOGE(TAG, "Unerwartetes Event");
    }
}


void app_main(void) {

    wifi_init_sta();

    xTaskCreate(animaltion_task, "animation_task", 4096, NULL, 5, NULL); // ( TaskName, TaskStackSize, TaskParameter, TaskPriority, TaskHandle
    xTaskCreate(presence_task, "presence_task", 4096, NULL, 5, NULL); // ( TaskName, TaskStackSize, TaskParameter, TaskPriority, TaskHandle

}



