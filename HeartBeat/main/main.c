
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

TaskHandle_t TestTask = NULL;
TaskHandle_t TestTask2 = NULL;

void PrintTask(void *parameter){
    while(1){
    printf("Task 1\n");
    vTaskDelay(2000/ portTICK_PERIOD_MS);}
}

void PrintTask2(void *parameter){
    while(1){
    printf("Task 2\n");
    vTaskDelay(2000/ portTICK_PERIOD_MS);}
}
void app_main(void)
{
    xTaskCreate(PrintTask, "Task Test", 2048, NULL, 1, &TestTask);
    xTaskCreate(PrintTask2, "Task Test", 2048, NULL, 1, &TestTask2);
    printf("Hello");
}
