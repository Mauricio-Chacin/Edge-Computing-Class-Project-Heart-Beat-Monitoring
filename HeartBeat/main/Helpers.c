#include "driver/i2c_master.h"
#include "Drivers.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//Handle Variables for I2C
i2c_master_bus_handle_t busSensor;
i2c_master_bus_handle_t busOled;
i2c_master_dev_handle_t Temp_dev;
i2c_master_dev_handle_t Heart_dev;
i2c_master_dev_handle_t Oled_dev;
//Oled Screen bitmaps

void i2c_init()
{
    i2c_master_bus_config_t bus_cfg_one = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDAONE,
        .scl_io_num = I2C_SCLONE,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg_one, &busSensor));

    i2c_master_bus_config_t bus_cfg_two = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_1,
        .sda_io_num = I2C_SDATWO,
        .scl_io_num = I2C_SCLTWO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg_two, &busOled));


    i2c_device_config_t sensor_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 100000,
    };
    i2c_device_config_t Oled_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = OLED_FREQ,
    };
    sensor_cfg.device_address = TEMP_ADDR;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(busSensor, &sensor_cfg, &Temp_dev));

    sensor_cfg.device_address = HEART_ADDR;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(busSensor, &sensor_cfg, &Heart_dev));

    Oled_cfg.device_address = OLED_ADDR;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(busOled, &Oled_cfg, &Oled_dev));
}

esp_err_t read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len, I2C_TIMEOUT);
}

esp_err_t write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t data[2] = { reg, val };
    return i2c_master_transmit(dev, data, 2, I2C_TIMEOUT);
}

void Heart_init()
{
    uint8_t id = 0;
    read_reg(Heart_dev, 0xFF, &id, 1);
    printf("Heart Beat Sensor part ID: 0x%02X (expect 0x15)\n", id);

    write_reg(Heart_dev, 0x09, 0x40);          // reset
    
    vTaskDelay(pdMS_TO_TICKS(100));

    write_reg(Heart_dev, 0x04, 0x00);          // clear FIFO write pointer
    write_reg(Heart_dev, 0x05, 0x00);          // clear overflow counter
    write_reg(Heart_dev, 0x06, 0x00);          // clear FIFO read pointer

    write_reg(Heart_dev, 0x08, 0x1F);          // FIFO: no averaging, rollover on
    write_reg(Heart_dev, 0x09, 0x03);          // mode: SpO2 (red + IR)
    write_reg(Heart_dev, 0x0A, 0x27);          // 4096nA range, 100 Hz, 411us pulse
    write_reg(Heart_dev, 0x0C, 0x24);          // red LED current (~7 mA)
    write_reg(Heart_dev, 0x0D, 0x24);          // IR LED current (~7 mA)
}


void Oled_init(){
    write_reg(Oled_dev, 0x00,0xAE); // Display OFF

    write_reg(Oled_dev, 0x00,0xD5); // Set display clock
    write_reg(Oled_dev, 0x00,0x80);

    write_reg(Oled_dev, 0x00,0xA8); // Multiplex ratio
    write_reg(Oled_dev, 0x00,0x3F); // 64 rows

    write_reg(Oled_dev, 0x00,0xD3); // Display offset
    write_reg(Oled_dev, 0x00,0x00);

    write_reg(Oled_dev, 0x00,0x40); // Start line = 0

    write_reg(Oled_dev, 0x00,0x8D); // Charge pump
    write_reg(Oled_dev, 0x00,0x14);
    
    write_reg(Oled_dev, 0x00,0x20); // Memory addressing mode
    write_reg(Oled_dev, 0x00,0x00); // Horizontal addressing

    write_reg(Oled_dev, 0x00,0xA1); // Segment remapping
    write_reg(Oled_dev, 0x00,0xC8); // COM scan direction

    write_reg(Oled_dev, 0x00,0xDA); // COM pin configuration
    write_reg(Oled_dev, 0x00,0x12);

    write_reg(Oled_dev, 0x00,0x81); // Contrast
    write_reg(Oled_dev, 0x00,0x7F);

    write_reg(Oled_dev, 0x00,0xD9); // Pre-charge period
    write_reg(Oled_dev, 0x00,0xF1);

    write_reg(Oled_dev, 0x00,0xDB); // VCOMH deselect level
    write_reg(Oled_dev, 0x00,0x40);

    write_reg(Oled_dev, 0x00,0xA4); // Display follows RAM
    write_reg(Oled_dev, 0x00,0xA6); // Normal display (not inverted)

    write_reg(Oled_dev, 0x00,0xAF); // Display ON
}


