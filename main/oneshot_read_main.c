/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"

const static char *TAG = "PLURA-ADC-READ";

/*---------------------------------------------------------------
        ADC General Macros
---------------------------------------------------------------*/
// ADC2 Channels
#define V_BAT ADC_CHANNEL_3
#define FC_TEMP ADC_CHANNEL_4
#define MPC_TEMP ADC_CHANNEL_5

#define ADC_ATTEN ADC_ATTEN_DB_12

// WiFi AP configuration
#define AP_SSID "ESP32_ADC_Monitor"
#define AP_PASS "12345678"
#define AP_CHANNEL 1
#define MAX_STA_CONN 4

static int adc_raw[3][10];
static int voltage[3][10];
static int latest_voltage = 0;  // V_BAT calibrated
static int latest_raw_vbat = 0; // V_BAT raw
static int latest_raw_fc = 0;   // FC_TEMP raw
static int latest_raw_mpc = 0;  // MPC_TEMP raw

// ADC related
static adc_oneshot_unit_handle_t adc2_handle;
static adc_cali_handle_t adc2_cali_handle;
static bool do_calibration2;

static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void example_adc_calibration_deinit(adc_cali_handle_t handle);
static void adc_read_task(void *arg);
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t api_voltage_handler(httpd_req_t *req);

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

    // Initialize WiFi
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

    // Initialize ADC
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };

    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_2,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    do_calibration2 = example_adc_calibration_init(ADC_UNIT_2, V_BAT, ADC_ATTEN, &adc2_cali_handle);
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, V_BAT, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, FC_TEMP, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, MPC_TEMP, &config));

    // Start ADC reading task
    xTaskCreate(adc_read_task, "adc_read_task", 4096, NULL, 5, NULL);

    // Start HTTP server
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

    // Keep app running
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/*---------------------------------------------------------------
        ADC Reading Task
---------------------------------------------------------------*/
static void adc_read_task(void *arg)
{
    while (1)
    {
        // Read V_BAT (Battery Voltage)
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, V_BAT, &adc_raw[0][0]));
        latest_raw_vbat = adc_raw[0][0];
        ESP_LOGI(TAG, "ADC%d Channel[%d] V_BAT Raw Data: %d", ADC_UNIT_2 + 1, V_BAT, adc_raw[0][0]);

        if (do_calibration2)
        {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_handle, adc_raw[0][0], &voltage[0][0]));
            latest_voltage = voltage[0][0] * 2;        // Multiply by 2
            float voltage_v = latest_voltage / 1000.0; // Convert to volts
            ESP_LOGI(TAG, "ADC%d Channel[%d] V_BAT Cali Voltage: %.2f V", ADC_UNIT_2 + 1, V_BAT, voltage_v);
        }

        // Read FC_TEMP (Fuel Cell Temperature)
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, FC_TEMP, &adc_raw[1][0]));
        latest_raw_fc = adc_raw[1][0];
        ESP_LOGI(TAG, "ADC%d Channel[%d] FC_TEMP Raw Data: %d", ADC_UNIT_2 + 1, FC_TEMP, adc_raw[1][0]);

        // Read MPC_TEMP (MPC Temperature)
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, MPC_TEMP, &adc_raw[2][0]));
        latest_raw_mpc = adc_raw[2][0];
        ESP_LOGI(TAG, "ADC%d Channel[%d] MPC_TEMP Raw Data: %d", ADC_UNIT_2 + 1, MPC_TEMP, adc_raw[2][0]);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*---------------------------------------------------------------
        HTTP Request Handler
---------------------------------------------------------------*/
static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html_page = "<!DOCTYPE html>"
                            "<html>"
                            "<head>"
                            "<title>ESP32 System Monitor</title>"
                            "<meta charset=\"UTF-8\">"
                            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
                            "<style>"
                            "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; display: flex; justify-content: center; align-items: center; }"
                            ".container { background: white; border-radius: 10px; padding: 30px; box-shadow: 0 10px 25px rgba(0,0,0,0.2); text-align: center; max-width: 500px; }"
                            "h1 { color: #333; margin-top: 0; margin-bottom: 30px; }"
                            ".reading-group { background: #f8f9fa; border-radius: 8px; padding: 20px; margin-bottom: 20px; }"
                            ".reading-label { color: #666; font-size: 14px; margin-bottom: 8px; font-weight: 500; }"
                            ".reading-value { font-size: 36px; font-weight: bold; color: #667eea; margin: 10px 0; }"
                            ".reading-raw { font-size: 12px; color: #999; margin-top: 8px; }"
                            ".status { color: #27ae60; font-size: 12px; margin-top: 20px; }"
                            "</style>"
                            "</head>"
                            "<body>"
                            "<div class=\"container\">"
                            "<h1>Battery Voltage</h1>"
                            "<div class=\"reading-group\">"
                            "<div class=\"reading-label\">Battery Voltage</div>"
                            "<div class=\"reading-value\" id=\"voltage\">-- V</div>"
                            "<div class=\"reading-raw\">Raw: <span id=\"raw-vbat\">--</span></div>"
                            "</div>"
                            "<div class=\"reading-group\">"
                            "<div class=\"reading-label\">FC Temperature</div>"
                            "<div class=\"reading-value\" id=\"fc-temp\">-- mV</div>"
                            "<div class=\"reading-raw\">Raw ADC: <span id=\"raw-fc\">--</span></div>"
                            "</div>"
                            "<div class=\"reading-group\">"
                            "<div class=\"reading-label\">MPC Temperature</div>"
                            "<div class=\"reading-value\" id=\"mpc-temp\">-- mV</div>"
                            "<div class=\"reading-raw\">Raw ADC: <span id=\"raw-mpc\">--</span></div>"
                            "</div>"
                            "<div class=\"status\">Auto-updating every 1 second</div>"
                            "</div>"
                            "<script>"
                            "function updateReading() {"
                            "  fetch('/api/voltage')"
                            "    .then(response => response.json())"
                            "    .then(data => {"
                            "      document.getElementById('voltage').textContent = (data.voltage / 1000).toFixed(2) + ' V';"
                            "      document.getElementById('raw-vbat').textContent = data.raw_vbat;"
                            "      document.getElementById('fc-temp').textContent = data.raw_fc + ' mV';"
                            "      document.getElementById('raw-fc').textContent = data.raw_fc;"
                            "      document.getElementById('mpc-temp').textContent = data.raw_mpc + ' mV';"
                            "      document.getElementById('raw-mpc').textContent = data.raw_mpc;"
                            "    })"
                            "    .catch(error => console.error('Error:', error));"
                            "}"
                            "updateReading();"
                            "setInterval(updateReading, 1000);"
                            "</script>"
                            "</body>"
                            "</html>";

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

/*---------------------------------------------------------------
        HTTP API Handler
---------------------------------------------------------------*/
static esp_err_t api_voltage_handler(httpd_req_t *req)
{
    char json_response[200];
    snprintf(json_response, sizeof(json_response),
             "{\"voltage\":%d,\"raw_vbat\":%d,\"raw_fc\":%d,\"raw_mpc\":%d}",
             latest_voltage, latest_raw_vbat, latest_raw_fc, latest_raw_mpc);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));
    return ESP_OK;
}

/*---------------------------------------------------------------
        ADC Calibration
---------------------------------------------------------------*/
static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}
