#ifndef Drivers_H
#define Drivers_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include <driver/i2c_master.h>
#include <driver/gpio.h>
//MACROS FOR I2C COMMUNICATION Bus 1
#define I2C_SDAONE       GPIO_NUM_21
#define I2C_SCLONE       GPIO_NUM_22
#define TEMP_ADDR  0x18
#define HEART_ADDR 0x57
//MACROS FOR I2C COMMUNICATION BUS 2
#define I2C_SDATWO       GPIO_NUM_26
#define I2C_SCLTWO       GPIO_NUM_25
#define OLED_ADDR 0x3C
#define OLED_FREQ 400000
#define OLED_W 128
#define OLED_H 64
#define I2C_TIMEOUT   100   // ms

//Filters
#define K 10
#define ALPHA 0.5

//BLE UUIDS
// Service: 4fafc201-1fb5-459e-8fcc-c5c9c331914b
#define SERVICE_UUID \
    0x4B, 0x91, 0x31, 0xC3, 0xC9, 0xC5, 0xCC, 0x8F, \
    0x9E, 0x45, 0xB5, 0x1F, 0x01, 0xC2, 0xAF, 0x4F

// Characteristic: beb5483e-36e1-4688-b7f5-ea07361b26a8
#define CHAR_UUID \
    0xA8, 0x26, 0x1B, 0x36, 0x07, 0xEA, 0xF5, 0xB7, \
    0x88, 0x46, 0xE1, 0x36, 0x3E, 0x48, 0xB5, 0xBE

#define BLE_PAYLOAD_MAX 16
#define BLE_DEVICE_NAME "HeartBeatMonitor"

//Handle Variables for I2C
i2c_master_bus_handle_t busSensor;
i2c_master_bus_handle_t busOled;
i2c_master_dev_handle_t Temp_dev;
i2c_master_dev_handle_t Heart_dev;
i2c_master_dev_handle_t Oled_dev;
//Prototypes for I2C functions

void i2c_init(void);
esp_err_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t len);
esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val);

void Heart_init(void);
void Oled_init(void);

//Reading or Actions related to I2C
float Temp_read(void);
bool Heart_read_sample(uint32_t *red, uint32_t *ir);
//Filters
uint32_t MovingAverage(uint32_t sample);
float EMAFilter(float sample);
//Oled
void OLED_WriteBuffer(i2c_master_dev_handle_t dev, const uint8_t *bitmap);
void horizontal_to_pages(uint8_t *src, uint8_t *dst);
//BLE Functions
void ble_start(void);
int ble_send_value(const void *data, uint16_t len);
#endif
