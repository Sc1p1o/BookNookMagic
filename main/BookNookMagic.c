#include "BookNookMagic.h"

typedef struct {
    char *data;
    size_t length;
} http_response_buffer_t;

typedef struct {
    uint8_t *data;
    size_t size;
} image_buffer_t;



bool g_server_online = false;
const char *TAG = "BookNookMagic";
char http_response_buffer[4096];
int http_response_length = 0;
EventGroupHandle_t wifi_event_group = NULL;

static spi_device_handle_t g_tft_spi;

static int s_retry_num = 0;

esp_lcd_panel_handle_t panel_handle = NULL;




void trim_newline(char *str)
{
    if (str == NULL) {
        return;
    }

    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

int uart_read_line(char *buffer, size_t max_len, bool echo, bool mask_input)
{
    if (buffer == NULL || max_len == 0) {
        return -1;
    }

    size_t index = 0;
    uint8_t ch = 0;

    while (1) {
        int len = uart_read_bytes(UART_NUM_0, &ch, 1, portMAX_DELAY);
        if (len <= 0) {
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            uart_write_bytes(UART_NUM_0, "\r\n", 2);
            break;
        }

        if ((ch == '\b' || ch == 127) && index > 0) {
            index--;
            if (echo || mask_input) {
                uart_write_bytes(UART_NUM_0, "\b \b", 3);
            }
            continue;
        }

        if (index < (max_len - 1)) {
            buffer[index++] = (char)ch;

            if (echo) {
                uart_write_bytes(UART_NUM_0, (const char *)&ch, 1);
            } else if (mask_input) {
                uart_write_bytes(UART_NUM_0, "*", 1);
            }
        }
    }

    buffer[index] = '\0';
    return (int)index;
}

void uart_console_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        .source_clk = UART_SCLK_DEFAULT,
#endif
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
}

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Verbindung getrennt, neuer Versuch %d/5", s_retry_num);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAILED_BIT);
            ESP_LOGE(TAG, "WLAN-Verbindung fehlgeschlagen");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "IP erhalten: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}



void wifi_init_sta(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL
    ));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        NULL
    ));

    wifi_config_t wifi_config = {0};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_connect_from_console(void)
{
    char ssid[33] = {0};
    char password[65] = {0};
    wifi_config_t wifi_config = {0};

    uart_write_bytes(UART_NUM_0, "\r\n=== WLAN-Konfiguration ===\r\n", 31);
    uart_write_bytes(UART_NUM_0, "SSID eingeben: ", 15);
    uart_read_line(ssid, sizeof(ssid), true, false);
    trim_newline(ssid);

    uart_write_bytes(UART_NUM_0, "Passwort eingeben: ", 19);
    uart_read_line(password, sizeof(password), false, true);
    trim_newline(password);

    if (strlen(ssid) == 0) {
        ESP_LOGE(TAG, "SSID darf nicht leer sein");
        return;
    }

    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    s_retry_num = 0;
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT);

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Verbinde mit SSID: %s", ssid);

    EventBits_t bits = xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAILED_BIT,
        pdTRUE,
        pdFALSE,
        pdMS_TO_TICKS(20000)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WLAN erfolgreich verbunden");
    } else if (bits & WIFI_FAILED_BIT) {
        ESP_LOGE(TAG, "WLAN konnte nicht verbunden werden");
    } else {
        ESP_LOGE(TAG, "Timeout bei WLAN-Verbindung");
    }
}

void test_server_health(void)
{
    esp_http_client_config_t config = {
        .url = "http://api.m-miller.me/health",
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        g_server_online = false;
        return;
    }

    g_server_online = (esp_http_client_perform(client) == ESP_OK);
    ESP_LOGI(TAG, "Server online: %s", g_server_online ? "ja" : "nein");
    esp_http_client_cleanup(client);
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    http_response_buffer_t *buffer = (http_response_buffer_t *)evt->user_data;

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA: {
            char *new_data = realloc(buffer->data, buffer->length + evt->data_len + 1);
            if (new_data == NULL) {
                ESP_LOGE(TAG, "Kein Speicher fuer HTTP-Daten");
                return ESP_FAIL;
            }

            buffer->data = new_data;
            memcpy(buffer->data + buffer->length, evt->data, evt->data_len);
            buffer->length += evt->data_len;
            buffer->data[buffer->length] = '\0';
            break;
    }

    default:
        break;
    }

    return ESP_OK;
}

void fetch_and_print_image_links(void)
{
    http_response_buffer_t response = {
        .data = NULL,
        .length = 0
    };

    esp_http_client_config_t config = {
        .url = "http://api.m-miller.me/images",
        .event_handler = http_event_handler,
        .user_data = &response,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "HTTP-Client konnte nicht initialisiert werden");
        return;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP-Request fehlgeschlagen: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(response.data);
        return;
    }

    int status_code = esp_http_client_get_status_code(client);
    if (status_code != 200) {
        ESP_LOGE(TAG, "Unerwarteter HTTP-Status: %d", status_code);
        esp_http_client_cleanup(client);
        free(response.data);
        return;
    }

    if (response.data == NULL) {
        ESP_LOGE(TAG, "Leere Antwort erhalten");
        esp_http_client_cleanup(client);
        return;
    }

    cJSON *root = cJSON_Parse(response.data);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON konnte nicht geparst werden");
        esp_http_client_cleanup(client);
        free(response.data);
        return;
    }

    cJSON *images = cJSON_GetObjectItemCaseSensitive(root, "images");
    if (!cJSON_IsArray(images)) {
        ESP_LOGE(TAG, "'images' fehlt oder ist kein Array");
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        free(response.data);
        return;
    }

    cJSON *image = NULL;
    cJSON_ArrayForEach(image, images) {
        if (cJSON_IsString(image) && image->valuestring != NULL) {
            printf("%s\n", image->valuestring);
        }
    }

    cJSON_Delete(root);
    esp_http_client_cleanup(client);
    free(response.data);
}

void fill_solid_color(esp_lcd_panel_handle_t panel, uint16_t color_rgb565)
{
    static uint16_t line[TFT_H_RES];
    for (int x = 0; x < TFT_H_RES; x++) {
        line[x] = color_rgb565;
    }
    for (int y = 0; y < TFT_V_RES; y++) {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, y, TFT_H_RES, y + 1, line));
    }
}

void init_tft_panel(void)
{
    if (PIN_NUM_BCKL >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << PIN_NUM_BCKL,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));
        gpio_set_level(PIN_NUM_BCKL, 1);
    }

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

    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));

    ESP_LOGI(TAG, "Test colors (wenn das klappt, klappt auch die Flamme)");
}


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uart_console_init();
    wifi_init_sta();
    wifi_connect_from_console();

    while (!g_server_online)
    {
        test_server_health();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Server online");

    fetch_and_print_image_links();

    init_tft_panel();

    while (1)
    {
        fill_solid_color(panel_handle, 0x2F0F); // rot
        vTaskDelay(pdMS_TO_TICKS(500));
        fill_solid_color(panel_handle, 0xFFF0); // grün
        vTaskDelay(pdMS_TO_TICKS(500));
        fill_solid_color(panel_handle, 0xF0FF); // blau
        vTaskDelay(pdMS_TO_TICKS(500));
        fill_solid_color(panel_handle, 0x0000); // schwarz
    }
}

