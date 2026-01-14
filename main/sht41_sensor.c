#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "sensor_config.h"

const static char *TAG = "SHT41";

i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t sht41_dev_handle = NULL;

/*---------------------------------------------------------------
        I2C Master Initialization
---------------------------------------------------------------*/
esp_err_t i2c_master_init(void)
{
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &bus_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize I2C bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2C master bus initialized on port %d (SCL=%d, SDA=%d)",
             I2C_MASTER_NUM, I2C_MASTER_SCL_IO, I2C_MASTER_SDA_IO);

    // Add SHT41 device to bus once during initialization
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHT41_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(bus_handle, &dev_cfg, &sht41_dev_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add SHT41 device (addr=0x%02X) to bus: %s",
                 SHT41_ADDR, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SHT41 device added to I2C bus at address 0x%02X", SHT41_ADDR);

    // Probe device with a simple read to verify it's responding
    vTaskDelay(pdMS_TO_TICKS(100));
    uint8_t probe_buf[1] = {0};
    ret = i2c_master_receive(sht41_dev_handle, probe_buf, 1, -1);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "SHT41 device not responding on probe (expected on cold start): %s",
                 esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "SHT41 device probe successful");
    }

    return ESP_OK;
}

/*---------------------------------------------------------------
        SHT41 Sensor Read Function
---------------------------------------------------------------*/
esp_err_t sht41_read_sensor(float *temperature, float *humidity)
{
    if (bus_handle == NULL || sht41_dev_handle == NULL)
    {
        ESP_LOGE(TAG, "I2C bus or SHT41 device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t write_buf[1] = {0xFD};
    uint8_t read_buf[6] = {0};

    // Send measurement command (high precision mode)
    esp_err_t ret = i2c_master_transmit(sht41_dev_handle, write_buf, sizeof(write_buf), -1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to send SHT41 command: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for measurement to complete (SHT41 needs ~10ms, use 20ms for safety)
    vTaskDelay(pdMS_TO_TICKS(20));

    // Read 6 bytes (temp_MSB, temp_LSB, temp_CRC, humidity_MSB, humidity_LSB, humidity_CRC)
    ret = i2c_master_receive(sht41_dev_handle, read_buf, sizeof(read_buf), -1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read from SHT41: %s (error code: 0x%X)", esp_err_to_name(ret), ret);

        // Try to recover by re-initializing the device
        static int retry_count = 0;
        if (retry_count < 3)
        {
            retry_count++;
            ESP_LOGI(TAG, "Attempting to recover SHT41 connection (attempt %d/3)", retry_count);
            vTaskDelay(pdMS_TO_TICKS(100));
            return ret;
        }
        else
        {
            ESP_LOGE(TAG, "SHT41 recovery failed, giving up");
            retry_count = 0;
            return ret;
        }
    }

    // Reset retry counter on success
    static int retry_count = 0;
    retry_count = 0;

    // Convert raw values to temperature and humidity
    uint16_t temp_raw = (read_buf[0] << 8) | read_buf[1];
    uint16_t humidity_raw = (read_buf[3] << 8) | read_buf[4];

    // Temperature: -45 C + 175 C * (raw / 65535)
    *temperature = -45.0f + 175.0f * ((float)temp_raw / 65535.0f);

    // Humidity: -6% + 125% * (raw / 65535)
    *humidity = -6.0f + 125.0f * ((float)humidity_raw / 65535.0f);

    ESP_LOGI(TAG, "SHT41 - Temp: %.2f C, Humidity: %.2f %%", *temperature, *humidity);
    return ESP_OK;
}
