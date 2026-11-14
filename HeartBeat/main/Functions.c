#include "driver/i2c_master.h"
#include "Drivers.h"
#include "esp_err.h"



float Temp_read(void)
{
    uint8_t Buffer[2];
    if (read_reg(Temp_dev, 0x05, Buffer, 2) != ESP_OK) return -999.0f;

    uint8_t upper = Buffer[0] & 0x1F;   // clear flag bits
    float temp;
    if (upper & 0x10) {            // sign bit set -> negative
        upper &= 0x0F;
        temp = 256.0f - (upper * 16.0f + Buffer[1] / 16.0f);
    } else {
        temp = upper * 16.0f + Buffer[1] / 16.0f;
    }
    return temp;
}

bool Heart_read_sample(uint32_t *red, uint32_t *ir)
{
    uint8_t wr, rd;
    read_reg(Heart_dev, 0x04, &wr, 1);
    read_reg(Heart_dev, 0x06, &rd, 1);
    if (((wr - rd) & 0x1F) == 0) return false;   // FIFO empty

    uint8_t b[6];
    if (read_reg(Heart_dev, 0x07, b, 6) != ESP_OK) return false;

    *red = (((uint32_t)b[0] << 16) | (b[1] << 8) | b[2]) & 0x3FFFF;
    *ir  = (((uint32_t)b[3] << 16) | (b[4] << 8) | b[5]) & 0x3FFFF;
    return true;
}