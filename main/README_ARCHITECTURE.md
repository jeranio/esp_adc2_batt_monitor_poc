# PLURA ESP32-S3 ADC Test - File Structure

## Overview

The monolithic `oneshot_read_main.c` has been refactored into modular components for better maintainability and organization.

## File Organization

### Header File

- **sensor_config.h** - Central configuration file containing:
  - All macro definitions (ADC channels, I2C pins, WiFi credentials)
  - External variable declarations
  - Function declarations for all modules

### Implementation Modules

1. **sht41_sensor.c** - SHT41 Temperature/Humidity Sensor

   - `i2c_master_init()` - Initializes I2C master bus
   - `sht41_read_sensor()` - Reads temperature and humidity via I2C
   - Temperature: -45°C + 175°C × (raw / 65535)
   - Humidity: -6% + 125% × (raw / 65535)

2. **adc_handler.c** - ADC Configuration and Reading

   - `adc_init()` - Initializes ADC2 with V_BAT and FC_TEMP channels
   - `example_adc_calibration_init()` - Sets up ADC calibration
   - `example_adc_calibration_deinit()` - Cleans up calibration
   - `adc_read_task()` - FreeRTOS task that reads ADC and SHT41 every 1 second
   - Stores calibrated voltage (V_BAT × 2 multiplier) and raw ADC values

3. **http_handlers.c** - HTTP Server Endpoints

   - `root_handler()` - Serves HTML dashboard at `/`
   - `api_voltage_handler()` - Returns JSON sensor data at `/api/voltage`
   - JSON format: `{voltage, raw_vbat, raw_fc, temperature, humidity}`

4. **oneshot_read_main.c** - Main Application Entry Point
   - `app_main()` - Application initialization orchestrator
   - `wifi_init_ap()` - WiFi AP mode setup
   - `http_server_init()` - HTTP server registration
   - Clean, readable initialization sequence

## Hardware Configuration

- **Microcontroller**: ESP32-S3
- **ADC Unit**: ADC2 (2 channels)
  - V_BAT: ADC_CHANNEL_3
  - FC_TEMP: ADC_CHANNEL_4
- **I2C Bus**:
  - SCL: GPIO48
  - SDA: GPIO47
  - Frequency: 400 kHz
- **SHT41 Sensor**: I2C Address 0x44
- **WiFi AP**: SSID "ESP32_ADC_Monitor", Password "12345678"

## Compilation

All modules compile with zero errors. The modular structure allows independent testing and development of each component.

## Building

The project uses CMake. CMakeLists.txt should be updated to reference the new source files if not using glob patterns.
