#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "zigbee.h"
#include "scd4x_i2c.h"
#include "sensirion_i2c_hal.h"
#include "config.h"
#include "tasks.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "zcl/esp_zigbee_zcl_power_config.h"


static const char *TAG = "sdc41_task";

RTC_DATA_ATTR bool scd4x_initialized = false;

static uint8_t battery_read(adc_oneshot_unit_handle_t adc, adc_cali_handle_t cali)
{
    int raw, mv;
    adc_oneshot_read(adc, BATTERY_ADC_CHANNEL, &raw);
    adc_cali_raw_to_voltage(cali, raw, &mv);
    int actual_mv = mv * BATTERY_VOLTAGE_DIVIDER;
    ESP_LOGI(TAG, "Battery: %d mV", actual_mv);
    if (actual_mv >= BATTERY_FULL_MV) return 200;
    if (actual_mv <= BATTERY_EMPTY_MV) return 0;
    return (uint8_t)((actual_mv - BATTERY_EMPTY_MV) * 200 / (BATTERY_FULL_MV - BATTERY_EMPTY_MV));
}

void sdc41_task(void *pvParameters)
{
    int16_t error = 0;

    // Init ADC for battery voltage sensing
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t adc_cali_handle;
    adc_oneshot_unit_init_cfg_t adc_unit_cfg = { .unit_id = BATTERY_ADC_UNIT };
    adc_oneshot_new_unit(&adc_unit_cfg, &adc_handle);
    adc_oneshot_chan_cfg_t adc_chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(adc_handle, BATTERY_ADC_CHANNEL, &adc_chan_cfg);
    adc_cali_curve_fitting_config_t adc_cali_cfg = {
        .unit_id = BATTERY_ADC_UNIT,
        .chan = BATTERY_ADC_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_curve_fitting(&adc_cali_cfg, &adc_cali_handle);

    sensirion_i2c_hal_init(SDC4X_SDA_PIN, SDC4X_SCL_PIN);
    if(!scd4x_initialized) {
        // Clean up potential SCD40 states
        ESP_LOGI(TAG, "First boot after power cycle. Resetting SCD4x sensor state, some i2c errors are expected");
        scd4x_wake_up();
        scd4x_stop_periodic_measurement();
        scd4x_reinit();

        uint16_t serial_0;
        uint16_t serial_1;
        uint16_t serial_2;
        error = scd4x_get_serial_number(&serial_0, &serial_1, &serial_2);
        if (error) {
            ESP_LOGE(TAG, "Error executing scd4x_get_serial_number(): %i", error);
        } else {
            ESP_LOGI(TAG, "serial: 0x%04x%04x%04x", serial_0, serial_1, serial_2);
        }
        scd4x_initialized = true;
    } else {
        ESP_LOGI(TAG, "Wakeup from deep sleep. Skipping initialization of SCD4x sensor");
    }

    uint16_t co2;
    int32_t temperature;
    int32_t humidity;
    int16_t zb_temperature;
    int16_t zb_humidity;
    float_t zb_co2;
    float_t sanity_temperature;
    float_t sanity_humidity;
    bool plausible;
    bool measurement_running = false;
    bool data_ready_flag;

    while (1)
    {
        data_ready_flag = false;
        if(!measurement_running) {
            sensirion_i2c_hal_reset_bus();
            ESP_LOGI(TAG, "Triggering SCD4x single shot measurement...");
            xTaskNotify(xZigbeeTask, CO2_MEASUREMENT_PENDING, eSetValueWithOverwrite);
            measurement_running = true;
            error = scd4x_measure_single_shot();
            if (error) {
                ESP_LOGE(TAG, "Error executing scd4x_measure_single_shot(): %i", error);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                measurement_running = false;
                continue;
            }
        }
        error = scd4x_get_data_ready_flag(&data_ready_flag);
        if (error) {
            ESP_LOGE(TAG, "Error executing scd4x_get_data_ready_flag(): %i", error);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            measurement_running = false;
            continue;
        }
        if (!data_ready_flag) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }
        measurement_running = false;

        error = scd4x_read_measurement(&co2, &temperature, &humidity);
        if (error) {
            ESP_LOGE(TAG, "Error executing scd4x_read_measurement(): %i", error);
        } else {
            plausible = true;
            zb_temperature = temperature/10;
            zb_humidity = humidity/10;
            zb_co2 = (float_t)co2/1000000.0f;
            sanity_temperature = zb_temperature/100.0;
            sanity_humidity = zb_humidity/100.0;
            ESP_LOGI(TAG, "Hum: %.1f %%; Tmp: %.1f °C; CO2: %d ppm", sanity_humidity, sanity_temperature, co2);
            if(sanity_temperature >= TEMP_MIN && sanity_temperature <= TEMP_MAX) {
                reportAttribute(HA_ESP_CO2_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &zb_temperature, 2);
            } else {
                ESP_LOGW(TAG, "Temperature value is implausible and no Zigbee update will be sent");
                plausible = false;
            }
            if(sanity_humidity >= HUMID_MIN && sanity_humidity <= HUMID_MAX) {
                reportAttribute(HA_ESP_CO2_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &zb_humidity, 2);
            } else {
                ESP_LOGW(TAG, "Humidity value is implausible and no Zigbee update will be sent");
                plausible = false;
            }
            if(co2 >= CO2_MIN && co2 <= CO2_MAX) {
                reportAttribute(HA_ESP_CO2_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID, &zb_co2, 4);
            } else {
                ESP_LOGW(TAG, "CO2 value is implausible and no Zigbee update will be sent");
                plausible = false;
            }
            if(plausible) {
                uint8_t batt_pct = battery_read(adc_handle, adc_cali_handle);
                uint8_t batt_voltage = (uint8_t)((BATTERY_EMPTY_MV + (uint32_t)batt_pct * (BATTERY_FULL_MV - BATTERY_EMPTY_MV) / 200) / 100);
                reportAttribute(HA_ESP_CO2_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_VOLTAGE_ID, &batt_voltage, 1);
                reportAttribute(HA_ESP_CO2_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_POWER_CONFIG, ESP_ZB_ZCL_ATTR_POWER_CONFIG_BATTERY_PERCENTAGE_REMAINING_ID, &batt_pct, 1);
                xTaskNotify(xZigbeeTask, CO2_MEASUREMENT_DONE, eSetValueWithOverwrite);
                vTaskDelay(MEASURE_INTERVAL_MS / portTICK_PERIOD_MS);
            }
        }
    }
}