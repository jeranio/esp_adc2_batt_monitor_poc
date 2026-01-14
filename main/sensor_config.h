#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_http_server.h"

/*---------------------------------------------------------------
        ADC General Macros
---------------------------------------------------------------*/
#define V_BAT ADC_CHANNEL_3
#define FC_TEMP ADC_CHANNEL_4
#define ADC_ATTEN ADC_ATTEN_DB_12

/*---------------------------------------------------------------
        SHT41 I2C Configuration
---------------------------------------------------------------*/
#define I2C_MASTER_SCL_IO 5
#define I2C_MASTER_SDA_IO 4
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000
#define SHT41_ADDR 0x44

/*---------------------------------------------------------------
        WiFi AP Configuration
---------------------------------------------------------------*/
#define AP_SSID "ESP32_ADC_Monitor"
#define AP_PASS "12345678"
#define AP_CHANNEL 1
#define MAX_STA_CONN 4

/*---------------------------------------------------------------
        Global Variables
---------------------------------------------------------------*/
extern int adc_raw[2][10];
extern int voltage[2][10];
extern int latest_voltage;
extern int latest_raw_vbat;
extern int latest_raw_fc;
extern float latest_humidity;
extern float latest_temperature;

extern i2c_master_bus_handle_t bus_handle;
extern adc_oneshot_unit_handle_t adc2_handle;
extern adc_cali_handle_t adc2_cali_handle;
extern bool do_calibration2;

/*---------------------------------------------------------------
        Function Declarations
---------------------------------------------------------------*/
// I2C initialization
esp_err_t i2c_master_init(void);

// SHT41 sensor
esp_err_t sht41_read_sensor(float *temperature, float *humidity);

// ADC calibration
bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
void example_adc_calibration_deinit(adc_cali_handle_t handle);

// ADC reading
void adc_read_task(void *arg);
esp_err_t adc_init(void);

// HTTP handlers
esp_err_t root_handler(httpd_req_t *req);
esp_err_t api_voltage_handler(httpd_req_t *req);

#endif // SENSOR_CONFIG_H
