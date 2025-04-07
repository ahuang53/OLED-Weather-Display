/*
This file is used to connect and check wifi status, as well as access the Open Meteo weather api 
*/

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "cJSON.h"
#include "creds.h"

//API URL
#define API_URL "http://api.open-meteo.com/v1/forecast?latitude=40.7799&longitude=-73.8051&current=temperature_2m,precipitation,weather_code&timezone=America%2FNew_York&temperature_unit=fahrenheit&precipitation_unit=inch"
//precipiation amount, temperature, weather code in NYC, New York in Fahrenheit and inches of precipitation

//Static variables
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
#define HTTP_BUFFER_MAX 1024

static char response_buffer[HTTP_BUFFER_MAX];  // Store the HTTP response
static int response_len = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI("WiFi", "Wi-Fi started, connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW("WiFi", "Disconnected from Wi-Fi, retrying...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI("WiFi", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

void wifi_setup(){
    ESP_LOGI("WiFi", "WiFi intitializing");
    // Initialize NVS (Non-Volatile Storage)
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());  
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // Set Wi-Fi mode to STA (Station mode)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Set Wi-Fi credentials (SSID and password)
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK  // Enforce WPA2
        },
    };

    //Set config and start Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WiFi", "Waiting for Wi-Fi connection...");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI("WiFi", "Connected to Wi-Fi, ready to make API calls");
}

void check_wifi_status(){// Check the connection status periodically
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err == ESP_OK && ap_info.rssi != 0) {
        ESP_LOGI("WiFi", "Connected to Wi-Fi. Signal strength: %d", ap_info.rssi);
        // ESP_LOGI("DEBUG", "Post run: %d bytes", uxTaskGetStackHighWaterMark(NULL));
    } 
    else {
        ESP_LOGI("WiFi", "Not connected to Wi-Fi.");
    }
}



esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt) //event handler for GET request
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        if(response_len + evt->data_len < HTTP_BUFFER_MAX){ //response doesnt exceed buffer
            memcpy(response_buffer + response_len, evt->data, evt->data_len); //copy data to buffer
            response_len += evt->data_len;
            response_buffer[response_len] = '\0'; //null terminate 
        }
        else{
            ESP_LOGI("API","Buffer too small");
        }
        // ESP_LOGI("API","%.*s\n", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }
    return ESP_OK;
}


// Function to parse JSON and extract data
void process_json_response(const char *json_str, float* api_values){
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL)
    {
        ESP_LOGI("JSON","Failed to parse JSON\n");
        return;
    }
    //ESP_LOGI("JSON","Json obj %s",cJSON_Print(root));
    cJSON *values = cJSON_GetObjectItem(root, "current");

    cJSON *temp = cJSON_GetObjectItem(values, "temperature_2m");
    cJSON *precip = cJSON_GetObjectItem(values, "precipitation");
    cJSON *w_code = cJSON_GetObjectItem(values, "weather_code");
    //ESP_LOGI("JSON","Temperature: %.2f\n",temp->valuedouble);

    //ESP_LOGI("JSON","Temperature: %.1f \nPrec: %.2f\nWeather Code: %.1f\n",temp->valuedouble, precip->valuedouble, w_code->valuedouble);
    api_values[0] = 4;
    api_values[1] = temp->valuedouble;
    api_values[2] = precip->valuedouble;
    api_values[3] = w_code->valuedouble;

    cJSON_Delete(root);  // Free memory
}

void api_get(float* api_values){ //api get request
    //Reset buffer
    memset(response_buffer, 0, response_len);
    response_len = 0;

    esp_http_client_config_t config_get = {
        .url = API_URL,
        .method = HTTP_METHOD_GET,
        .cert_pem = NULL,
        .event_handler = client_event_get_handler};
        
    //Run http request
    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    // ESP_LOGI("API","DONE: %d",response_len);

    if (response_len > 0)  
    {
        process_json_response(response_buffer,api_values);
        ESP_LOGI("API","Content is done");
    }
}


