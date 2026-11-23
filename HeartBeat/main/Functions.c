#include "driver/i2c_master.h"
#include "Drivers.h"
#include "esp_err.h"
#include <string.h>
//Oled Screen bitmaps
uint8_t font5x7[12][5] = {
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    {0x00,0x60,0x60,0x00,0x00}, // .
    {0x08,0x08,0x08,0x08,0x08}, // -
};
// OLED bitmap 128x64



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

uint32_t HeartRateWindow[K];
int CountHeart = 0;
int IdxHeart = 0;
uint32_t Sum = 0;

// This function will be defunct since I updated the code to add the new filtering.
uint32_t MovingAverage(uint32_t sample){
    if(CountHeart == K){
        Sum -= HeartRateWindow[IdxHeart];  
    }
    else{
        CountHeart++;
    }
    HeartRateWindow[IdxHeart] = sample;
    Sum += sample;
    if(IdxHeart == K -1){
        IdxHeart = 0;
    }
    else{
    IdxHeart++;
    }
    return (Sum/CountHeart);
}
float PreviousEma = 0;

float EMAFilter(float sample){
    float Ema;
    if(!PreviousEma){
        PreviousEma = sample;
    return sample;
    }
    else{
    Ema =(ALPHA * sample) + (1 - ALPHA) * PreviousEma;
    PreviousEma = Ema;
    return Ema;
    }
}
int FrameBuffer[1024];
void fb_set_pixel(int x, int y)       // turn ON
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    FrameBuffer[(y / 8) * OLED_W + x] |= (1 << (y % 8));
}

void fb_clear_pixel(int x, int y)     // turn OFF
{
    if (x < 0 || x >= OLED_W || y < 0 || y >= OLED_H) return;
    FrameBuffer[(y / 8) * OLED_W + x] &= ~(1 << (y % 8));
}

void OLED_WriteBuffer(i2c_master_dev_handle_t dev, const uint8_t *bitmap)
{
    uint8_t line[OLED_W + 1];
    line[0] = 0x40;                          // 0x40 = data stream

    for (int page = 0; page < 7; page++) {
        write_reg(Oled_dev, 0x00, 0xB0 | page);          // set page
        write_reg(Oled_dev, 0x00, 0x00);                 // column low nibble = 0
        write_reg(Oled_dev, 0x00, 0x10);                 // column high nibble = 0

        memcpy(&line[1], &bitmap[page * OLED_W], OLED_W);
        ESP_ERROR_CHECK(i2c_master_transmit(dev, line, sizeof(line), -1));
    }
}

void horizontal_to_pages(uint8_t *src, uint8_t *dst)
{
    const int stride = OLED_W / 8;         // bytes per row in source
    memset(dst, 0, OLED_W * OLED_H / 8);

    for (int y = 0; y < OLED_H; y++) {
        for (int x = 0; x < OLED_W; x++) {
            uint8_t byte = src[y * stride + (x / 8)];
            if (byte & (0x80 >> (x % 8))) {              // MSB = leftmost pixel
                dst[(y / 8) * OLED_W + x] |= (1 << (y % 8));  // LSB = top of page
            }
        }
    }
}
void OLED_Clear()
{
    int length = 1023;
    uint8_t buffer[length];

    buffer[0] = 0x40; // Data control byte

    for (int i = 0; i < length; i++)
        buffer[i + 1] = 0;

    i2c_master_transmit(Oled_dev, buffer,
                        length + 1, -1);
}
