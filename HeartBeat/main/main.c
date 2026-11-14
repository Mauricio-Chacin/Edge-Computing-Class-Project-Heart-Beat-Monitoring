#include <stdio.h>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "drivers.h"
#include "Functions.h"

int count = 0;
int idx = 0;
float Sum = 0;
float TempWindow[Window];
float HeartRateWindow[Window];
float OxygenWindow[Window];


static void sensor_task(void *arg)
{
    uint32_t red = 0, ir = 0;
    while (1) {
        while (Heart_read_sample(&red, &ir)) { }   // drain FIFO, keep latest
        float temp = Temp_read();
        float filter = moving_average(temp,&count,&Sum,&idx,HeartRateWindow);
        printf("Temp: %.2f C | RED: %lu | IR: %lu\t filter: %.2f\n",
               temp, (unsigned long)red, (unsigned long)ir,filter);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main(void)
{
    i2c_init();
    Heart_init();
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}