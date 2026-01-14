#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "sensor_config.h"

const static char *TAG = "HTTP";

/*---------------------------------------------------------------
        Root Handler (HTML Dashboard)
---------------------------------------------------------------*/
esp_err_t root_handler(httpd_req_t *req)
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
                            "<h1>PLURA System Monitor</h1>"
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
                            "<div class=\"reading-label\">SHT41 Temperature</div>"
                            "<div class=\"reading-value\" id=\"sht41-temp\">-- C</div>"
                            "</div>"
                            "<div class=\"reading-group\">"
                            "<div class=\"reading-label\">SHT41 Humidity</div>"
                            "<div class=\"reading-value\" id=\"sht41-humidity\">-- %</div>"
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
                            "      document.getElementById('sht41-temp').textContent = data.temperature.toFixed(2) + ' C';"
                            "      document.getElementById('sht41-humidity').textContent = data.humidity.toFixed(2) + ' %';"
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
    ESP_LOGI(TAG, "Sent HTML dashboard");
    return ESP_OK;
}

/*---------------------------------------------------------------
        API Handler (JSON Response)
---------------------------------------------------------------*/
esp_err_t api_voltage_handler(httpd_req_t *req)
{
    char json_response[300];
    snprintf(json_response, sizeof(json_response),
             "{\"voltage\":%d,\"raw_vbat\":%d,\"raw_fc\":%d,\"temperature\":%.2f,\"humidity\":%.2f}",
             latest_voltage, latest_raw_vbat, latest_raw_fc, latest_temperature, latest_humidity);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_response, strlen(json_response));
    return ESP_OK;
}
