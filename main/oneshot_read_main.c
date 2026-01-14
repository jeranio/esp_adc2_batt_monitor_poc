/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "sensor_config.h"

const static char *TAG = "APP";

/*---------------------------------------------------------------
        WiFi Initialization
---------------------------------------------------------------*/
static void wifi_init_ap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .channel = AP_CHANNEL,
            .password = AP_PASS,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WiFi AP started. SSID: %s, Password: %s", AP_SSID, AP_PASS);
}

/*---------------------------------------------------------------
        HTTP Server Initialization
---------------------------------------------------------------*/
static void http_server_init(void)
{
    httpd_config_t config_http = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(httpd_start(&server, &config_http));

    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL};

    httpd_uri_t uri_api = {
        .uri = "/api/voltage",
        .method = HTTP_GET,
        .handler = api_voltage_handler,
        .user_ctx = NULL};

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_root));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &uri_api));
    ESP_LOGI(TAG, "HTTP server started on port %d", config_http.server_port);
}

/*---------------------------------------------------------------
        Main Application Entry Point
---------------------------------------------------------------*/
void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS Flash initialized");

    // Initialize WiFi
    wifi_init_ap();

    // Initialize I2C for SHT41
    ESP_ERROR_CHECK(i2c_master_init());

    // Initialize ADC
    ESP_ERROR_CHECK(adc_init());

    // Start ADC reading task
    xTaskCreate(adc_read_task, "adc_read_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "ADC reading task started");

    // Start HTTP server
    http_server_init();

    // Keep app running
    ESP_LOGI(TAG, "Application initialization complete");
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
