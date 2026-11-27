#include "driver/i2c_master.h"
#include "Drivers.h"
#include "esp_err.h"
#include <string.h>

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
void set_cursor(uint8_t x, uint8_t page) {
    write_reg(Oled_dev,0x00,0xB0 | (page & 7));
    write_reg(Oled_dev,0x00,x & 0x0F);
    write_reg(Oled_dev,0x00,0x10 | (x >> 4));   
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

void PrintNum(int pos, int digit,uint8_t *Bitmap){
    int Start = 0;
    uint8_t TempBuf[1024];
    uint8_t font5x8[10][8] = {
    {0x38,0x44,0x44,0x4C,0x54,0x64,0x44,0x38}, //0
    {0x10,0x30,0x50,0x10,0x10,0x10,0x10,0x7C}, // 1
    {0x78,0x0C,0x0C,0x0C,0x38,0x60,0x60,0x7C}, //2
    {0x7C,0x0C,0x0C,0x7C,0x7C,0x0C,0x0C,0x7C}, // 3
    {0x4C,0x4C,0x4C,0x7C,0x0C,0x0C,0x0C,0x0C},// 4
    {0x7C,0x60,0x60,0x7C,0x7C,0x0C,0x0C,0x7C}, //5
    {0x7C,0x40,0x40,0x40,0x7C,0x44,0x44,0x7C}, //6
    {0x7C,0x7C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C}, //7
    {0x38,0x44,0x44,0x44,0x7C,0x44,0x44,0x38}, // 8
    {0x38,0x44,0x44,0x44,0x7C,0x04,0x44,0x78} // 9
    };
    switch (pos)
    {
    case 1:
        Start = 154;
        break;
    case 2:
        Start = 155;
        break;
    case 3:
        Start = 156;
        break;
    case 4:
        Start = 362;
        break;
    case 5:
        Start = 363;
        break;
    case 6:
        Start = 364;
        break;   
    case 7:
        Start = 682;
        break;
    case 8:
        Start = 683;
        break;
    case 9:
        Start = 685;
        break;   
    }
    for(int i = 0; i < 8; i++){

        //for(int j = 0; j < 8; j++){
        Bitmap[Start + (i * 16)] = font5x8[digit][i];
        //}
    }
    horizontal_to_pages(Bitmap,TempBuf);
    OLED_WriteBuffer(Oled_dev,TempBuf);

}
void PrintReading(int Start,int *Reading,uint8_t *Bitmap){
    int Count = 0;
    for(int i = Start; i < (Start + 3); i++){
        PrintNum(i,Reading[Count],Bitmap);
        Count++;
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