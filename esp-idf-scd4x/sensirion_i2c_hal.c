/*
 * Copyright (c) 2018, Sensirion AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Sensirion AG nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "sensirion_i2c_hal.h"
#include "esp_err.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"
#include "sensirion_common.h"
#include "sensirion_config.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "esp_check.h"

#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_NO_ACK_CHECK 0
#define I2C_ACK_CHECK 1
#define I2C_ACK_VAL 0
#define I2C_NACK_VAL 1

static const char *TAG = "sensirion_i2c_hal";

static i2c_master_bus_handle_t* bus_handle = NULL;
static i2c_master_dev_handle_t* dev_handle = NULL;

/**
 * Select the current i2c bus by index.
 * All following i2c operations will be directed at that bus.
 *
 * THE IMPLEMENTATION IS OPTIONAL ON SINGLE-BUS SETUPS (all sensors on the same
 * bus)
 *
 * @param bus_idx   Bus index to select
 * @returns         0 on success, an error code otherwise
 */
int16_t sensirion_i2c_hal_select_bus(uint8_t bus_idx) {
    return NOT_IMPLEMENTED_ERROR;
}

/**
 * Initialize all hard- and software components that are needed for the I2C
 * communication.
 */
int16_t sensirion_i2c_hal_init(int gpio_sda, int gpio_scl) {
    bus_handle = calloc(1, sizeof(i2c_master_bus_handle_t));
    if(bus_handle == NULL) {
        ESP_RETURN_ON_ERROR(ESP_ERR_NO_MEM, TAG, "Failed to allocate memory for I2C bus handle");
    }
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT,
        .scl_io_num = gpio_scl,
        .sda_io_num = gpio_sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_mst_config, bus_handle), TAG, "Failed to initialize I2C bus");
    return ESP_OK;
}

static i2c_master_dev_handle_t* get_i2c_device_handle(uint8_t address) {
    if(dev_handle==NULL) {
        dev_handle = calloc(1, sizeof(i2c_master_dev_handle_t));
        if(dev_handle == NULL) {
            ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
        }
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = address,
            .scl_speed_hz = I2C_FREQ,
        };

        ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_cfg, dev_handle));
    }
    return dev_handle;
}

/**
 * Reset the I2C bus to recover from a stuck state after light sleep.
 */
int16_t sensirion_i2c_hal_reset_bus(void) {
    if (bus_handle != NULL) {
        return i2c_master_bus_reset(*bus_handle);
    }
    return ESP_OK;
}

/**
 * Release all resources initialized by sensirion_i2c_hal_init().
 */
int16_t sensirion_i2c_hal_free(void) {
    if(dev_handle != NULL){
        ESP_RETURN_ON_ERROR(i2c_master_bus_rm_device(*dev_handle), TAG, "Failed to remove device from I2C bus");
        free(dev_handle);
        dev_handle = NULL;
    }
    if(bus_handle != NULL){
        ESP_RETURN_ON_ERROR(i2c_del_master_bus(*bus_handle), TAG, "Failed to delete I2C bus");
        free(bus_handle);
        bus_handle = NULL;
    }
    return ESP_OK;
}

/**
 * Execute one read transaction on the I2C bus, reading a given number of bytes.
 * If the device does not acknowledge the read command, an error shall be
 * returned.
 *
 * @param address 7-bit I2C address to read from
 * @param data    pointer to the buffer where the data is to be stored
 * @param count   number of bytes to read from I2C and store in the buffer
 * @returns 0 on success, error code otherwise
 */
int16_t sensirion_i2c_hal_read(uint8_t address, uint8_t* data, uint16_t count) {
    return i2c_master_receive(*get_i2c_device_handle(address), data, count, 1000);
}

/**
 * Execute one write transaction on the I2C bus, sending a given number of
 * bytes. The bytes in the supplied buffer must be sent to the given address. If
 * the slave device does not acknowledge any of the bytes, an error shall be
 * returned.
 *
 * @param address 7-bit I2C address to write to
 * @param data    pointer to the buffer containing the data to write
 * @param count   number of bytes to read from the buffer and send over I2C
 * @returns 0 on success, error code otherwise
 */
int16_t sensirion_i2c_hal_write(uint8_t address, const uint8_t* data, uint16_t count) {
    return i2c_master_transmit(*get_i2c_device_handle(address), data, count, 1000);
}

/**
 * Sleep for a given number of microseconds. The function should delay the
 * execution for at least the given time, but may also sleep longer.
 *
 * Despite the unit, a <10 millisecond precision is sufficient.
 *
 * @param useconds the sleep time in microseconds
 */
void sensirion_i2c_hal_sleep_usec(uint32_t useconds) {
    /* TODO: Check if this actually works */
    uint32_t msec = useconds / 1000;
    if (useconds % 1000 > 0) {
        msec++;
    }
    vTaskDelay(msec / portTICK_PERIOD_MS);
}