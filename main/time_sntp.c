/*
This file is used to setup and run the time module
*/

#include "esp_log.h"
#include "esp_sntp.h"

static time_t current_time; 

//Initalize sntp and sync current time
float* sntp_start(){
    time_t now;
    struct tm timeinfo;

    ESP_LOGI("SNTP", "Initializing SNTP...");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");  // Use NTP server
    esp_sntp_init();
    ESP_LOGI("SNTP", "Initailizing done");

    //Make sure time is synced
    while(sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED){
        ESP_LOGI("TIME","Sync in progress");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ESP_LOGI("TIME","Sync done");

    time(&now);
    setenv("TZ","EST+5EDT,M3.2.0/2,M11.1.0/2",1); //set time to EST
    tzset();

    current_time = now;
    localtime_r(&current_time,&timeinfo);
    // ESP_LOGI("TIME", "Current time: %02d-%02d-%04d %02d:%02d:%02d",
    // timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
    // timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    static float timeData[6]; //max item size
    timeData[0] = 6;
    timeData[1] = timeinfo.tm_mday;
    timeData[2] = timeinfo.tm_mon + 1;
    timeData[3] = timeinfo.tm_year + 1900;
    timeData[4] = timeinfo.tm_hour;
    timeData[5] = timeinfo.tm_min;
    // for(int i = 0; i < 6; ++i){
    //     ESP_LOGI("SNTP","content: %02f",timeData[i]);
    // }

    return timeData;

    // time_t current; 
    // char time_buf[64];
    // struct tm timeinfo;

    // time(&current);
    // setenv("TZ","EST+5EDT,M3.2.0/2,M11.1.0/2",1); //set time to EST
    // tzset();

    // localtime_r(&current,&timeinfo);
    // strftime(time_buf, sizeof(time_buf), "%c",&timeinfo);
    // ESP_LOGI("TIME","Current time in NYC is: %s",time_buf);
}

//Increments the time by a second every time its called
//If seconds are reset to 0, one minute has passed and returns the time variable
time_t* increment_time(){
    static time_t last_minute_time = 0;

    current_time+= 1;
    struct tm timeinfo;
    localtime_r(&current_time,&timeinfo);

    if(timeinfo.tm_sec == 0){
        last_minute_time = current_time;
        return &last_minute_time;
    }
    return NULL;
}
