/*
This is the main file for the oled weather display project
*/

//Modules
#include "i2c_oled.h"
#include "weather_api.h"
#include "time_sntp.h"

//ESP/C Library
#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"

//Macros
#define QUEUE_LEN 5
#define MAX_ITEM_SIZE 6
#define API_SIZE 4
#define ONE_HOUR_MS (1000 * 60 * 60)

//Static Variables
static float api_values[API_SIZE];

//Handles
//static TimerHandle_t wifi_status = NULL;
static QueueHandle_t lvgl_queue;

//WiFi status check task
void wifi_status_task(void *parameter){ //temporary
    while(1){
        check_wifi_status();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void update_time(void *parameter){ 
    float timeData[MAX_ITEM_SIZE];
    while(1){
        time_t *minute = increment_time();
        if (minute != NULL) { //change time every minute
            ESP_LOGI("DEBUG", "Before: %d bytes", uxTaskGetStackHighWaterMark(NULL));
            memset(timeData,0,MAX_ITEM_SIZE); //clear timeData
            struct tm timeinfo;
            // ESP_LOGI("MINUTE", "New minute: %d",*minute);
            localtime_r(minute,&timeinfo);
            ESP_LOGI("TIME", "Current time: %02d-%02d-%04d %02d:%02d:%02d",
            timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            
            timeData[0] = MAX_ITEM_SIZE;
            timeData[1] = timeinfo.tm_mday;
            timeData[2] = timeinfo.tm_mon + 1;
            timeData[3] = timeinfo.tm_year + 1900;
            timeData[4] = timeinfo.tm_hour;
            timeData[5] = timeinfo.tm_min;
            // ESP_LOGI("DEBUG", "After: %d bytes", uxTaskGetStackHighWaterMark(NULL));
            xQueueSend(lvgl_queue, timeData, portMAX_DELAY);
            // lvgl_update(timeData);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS); //run every second
        
    }
}

void update_weather(void *parameter){ //Producer
    while(1){
        ESP_LOGI("DEBUG", "Run api get: %d bytes", uxTaskGetStackHighWaterMark(NULL));
        //memset(api_get,0,API_SIZE); //clear api get
        ESP_LOGI("DEBUG", "AFTER api get 1: %d bytes", uxTaskGetStackHighWaterMark(NULL));
        api_get(api_values);
        ESP_LOGI("DEBUG", "AFTER api get 2: %d bytes", uxTaskGetStackHighWaterMark(NULL));
        xQueueSend(lvgl_queue, api_values, portMAX_DELAY);
        vTaskDelay(ONE_HOUR_MS / portTICK_PERIOD_MS); //delay for 1 hour
    }
}

void send_to_lvgl(void *paramter){
    float temp[MAX_ITEM_SIZE];
    while(1){
        xQueueReceive(lvgl_queue, (void*)temp, portMAX_DELAY);
        lvgl_update(temp);
        vTaskDelay(1000 / portTICK_PERIOD_MS); //check every 0.8 seconds
    }
}

void app_main(void){ //setup function
    //Setup and Intialization
    oled_init();
    lvgl_init();
    wifi_setup();

    ESP_LOGI("MAIN","Intialization done");

    //Initial Update 
    lvgl_update(sntp_start()); //update lvgl with sntp init values
    //api_get(api_values);
    //lvgl_update(api_values);

    ESP_LOGI("MAIN","Inital Update Done");
    
    lvgl_queue = xQueueCreate(QUEUE_LEN, (sizeof(float)*MAX_ITEM_SIZE)); //max array size is 6

    ESP_LOGI("MAIN","Queue Made");

    //xTaskCreate(wifi_status_task,"Wifi Status Task", 2048, NULL, 0, NULL);
    xTaskCreate(update_time,"Get Time Task", 2048, NULL, 0, NULL); //Producer
    xTaskCreate(update_weather, "Get Weather Task",2248,NULL,0,NULL); //Producer
    xTaskCreate(send_to_lvgl, "Process Queue Items Task",2048,NULL,0,NULL); //Consumer

    ESP_LOGI("MAIN","All Tasks Made");

    vTaskDelete(NULL); //end current task 
} 