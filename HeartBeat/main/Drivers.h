#ifndef Drivers_H
#define Drivers_H
#include <driver/i2c_master.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
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

//Sampling and anomaly detection
#define SAMPLE_PERIOD_MS    100     // one sample every 100 ms (10 Hz)
#define ANOMALY_THRESHOLD   1.2f    // score above this = abnormal
#define ALERT_NEEDED        3       // alert if 3 of the last 5 windows were abnormal
#define ALERT_WINDOW        5
#define ANOMALY_MAX_ROWS    60      // size of the window buffer (the model uses 10 rows)
#define FINGER_IR_MIN        50000
#define FINGER_RED_MIN       30000
#define FINGER_LOST_SAMPLES  5      // 5 x 100 ms = 0.5 s below threshold => finger is gone

//Handle Variables for I2C
i2c_master_bus_handle_t busSensor;
i2c_master_bus_handle_t busOled;
i2c_master_dev_handle_t Temp_dev;
i2c_master_dev_handle_t Heart_dev;
i2c_master_dev_handle_t Oled_dev;

//Queue Structure
typedef struct {
    bool    finger;            // false => show "no finger"
    bool    alert;
    uint8_t bpm_digits[3];  uint8_t bpm_len;    // e.g. 113.4 -> {1,1,3} len 3 | 72.5 -> {7,2} len 2
    uint8_t spo2_digits[3]; uint8_t spo2_len;   // e.g. 98.4 -> {9,8} len 2 | 100 -> {1,0,0} len 3
    uint8_t temp_digits[3];                     // e.g. 36.52 -> {3,6,5} (draw the dot after digit 2)
} display_msg_t;
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
void set_cursor(uint8_t x, uint8_t page);
void PrintNum(int pos, uint8_t digit, uint8_t *Bitmap);
void PrintReading(int Start, const uint8_t *Reading, uint8_t len, uint8_t *Bitmap);
int split_digits(int v, uint8_t *out);
void OledShowReadings( const display_msg_t *m,uint8_t *Bitmap);
void OledClearReadings(uint8_t *Bitmap);
//Queue
void display_send(QueueHandle_t display,const display_msg_t *m);

#endif
