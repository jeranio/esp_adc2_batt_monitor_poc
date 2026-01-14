#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"
#include "sensor_config.h"

const static char *TAG = "ADC";

/*---------------------------------------------------------------
        ADC Global Variables
---------------------------------------------------------------*/
int adc_raw[2][10];
int voltage[2][10];
int latest_voltage = 0;
int latest_raw_vbat = 0;
int latest_raw_fc = 0;
float latest_humidity = 0.0;
float latest_temperature = 0.0;

adc_oneshot_unit_handle_t adc2_handle;
adc_cali_handle_t adc2_cali_handle;
bool do_calibration2;

/*---------------------------------------------------------------
        ADC Calibration Initialization
---------------------------------------------------------------*/
bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
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

/*---------------------------------------------------------------
        ADC Calibration Deinitialization
---------------------------------------------------------------*/
void example_adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

/*---------------------------------------------------------------
        ADC Initialization
---------------------------------------------------------------*/
esp_err_t adc_init(void)
{
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

    ESP_LOGI(TAG, "ADC2 initialized with 2 channels: V_BAT, FC_TEMP");
    return ESP_OK;
}

/*---------------------------------------------------------------
        ADC Reading Task
---------------------------------------------------------------*/
void adc_read_task(void *arg)
{
    extern esp_err_t sht41_read_sensor(float *temperature, float *humidity);

    while (1)
    {
        // Read V_BAT (Battery Voltage)
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, V_BAT, &adc_raw[0][0]));
        latest_raw_vbat = adc_raw[0][0];
        ESP_LOGI(TAG, "ADC2 Channel[%d] V_BAT Raw Data: %d", V_BAT, adc_raw[0][0]);

        if (do_calibration2)
        {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc2_cali_handle, adc_raw[0][0], &voltage[0][0]));
            latest_voltage = voltage[0][0] * 2;
            float voltage_v = latest_voltage / 1000.0;
            ESP_LOGI(TAG, "ADC2 Channel[%d] V_BAT Cali Voltage: %.2f V", V_BAT, voltage_v);
        }

        // Read FC_TEMP (Fuel Cell Temperature)
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, FC_TEMP, &adc_raw[1][0]));
        latest_raw_fc = adc_raw[1][0];
        ESP_LOGI(TAG, "ADC2 Channel[%d] FC_TEMP Raw Data: %d", FC_TEMP, adc_raw[1][0]);

        // Read SHT41 (Temperature and Humidity)
        if (sht41_read_sensor(&latest_temperature, &latest_humidity) == ESP_OK)
        {
            ESP_LOGI(TAG, "SHT41 - Temp: %.2f C, Humidity: %.2f %%", latest_temperature, latest_humidity);
        }
        else
        {
            ESP_LOGE(TAG, "Failed to read SHT41 sensor");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
