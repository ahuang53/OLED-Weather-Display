/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

/*
This file is used to setup and run the i2c OLED display
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "esp_mac.h"
#include "esp_lcd_panel_vendor.h"
#include "fonts/fonts.h"
#include "esp_log.h"

//Pins
#define PIN_NUM_SDA           GPIO_NUM_21
#define PIN_NUM_SCL           GPIO_NUM_22
#define I2C_HW_ADDR           0x3C

//Display dimensions
#define LCD_H_RES             128
#define LCD_V_RES             64

#define LCD_PIXEL_CLOCK_HZ    (400 * 1000)
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

#define I2C_BUS_PORT  0
#define buf_len 10

//Fonts
#define clear_sky "\xEF\x84\x91"
#define cloudy "\xEF\x83\x82"
#define drizzle "\xEF\x9C\xBD"
#define rain "\xEF\x9D\x80"
#define thunder "\xEF\x9D\x9A"
#define snow "\xEF\x8B\x9C"
#define precipipation "\xEF\x81\x83"

//Static variables
static esp_lcd_panel_handle_t panel_handle = NULL;
static lv_obj_t *scr = NULL;
static char buf[buf_len];

//LVGL Labels
static lv_obj_t *time = NULL;
static lv_obj_t *date = NULL;
static lv_obj_t *temp = NULL;
static lv_obj_t *precip = NULL;
static lv_obj_t *precip_img = NULL;
static lv_obj_t *weather = NULL;
static lv_obj_t *wea_label = NULL;

//Stores the name of a weather type and its macro font label
typedef struct { 
    const char* font_label;
    const char* name;
} WeatherLabel;

const WeatherLabel WeatherData[] = {
    {clear_sky,"Clear Sky"},
    {cloudy, "Cloudy"},
    {drizzle, "Drizzle"},
    {rain, "Rain"},
    {thunder, "Thunder"},
    {snow, "Snow"}
};

//setup oled and the lvgl
void oled_init(void)
{
    char* TAG = "OLED";
    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_handle_t i2c_bus = NULL;
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .i2c_port = I2C_BUS_PORT,
        .sda_io_num = PIN_NUM_SDA,
        .scl_io_num = PIN_NUM_SCL,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &i2c_bus));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = I2C_HW_ADDR,
        .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
        .control_phase_bytes = 1,               // According to SSD1306 datasheet
        .lcd_cmd_bits = LCD_CMD_BITS,   // According to SSD1306 datasheet
        .lcd_param_bits = LCD_CMD_BITS, // According to SSD1306 datasheet
        .dc_bit_offset = 6,             // According to SSD1306 datasheet
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &io_config, &io_handle));

    ESP_LOGI(TAG, "Install SSD1306 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .bits_per_pixel = 1,
        .reset_gpio_num = -1,
    };
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = LCD_V_RES,
    };
    panel_config.vendor_config = &ssd1306_config;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_LOGI(TAG, "Finshed OLED I2C initialization");

    TAG = "LVGL";
    lv_init();

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_V_RES,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        }
    };
    lv_disp_t*disp = lvgl_port_add_disp(&disp_cfg);
    ESP_LOGI(TAG, "Finished LVGL initialization");

    // Rotation of the screen
    lv_disp_set_rotation(disp, LV_DISP_ROT_180);
    scr = lv_disp_get_scr_act(disp);
}

void lvgl_init(void){ //creates all the labels for lvgl elements
    ESP_LOGI("LVGL", "Initalize LVGL Labels");
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (lvgl_port_lock(0)) {

        //Labels
        time = lv_label_create(scr);
        date = lv_label_create(scr);
        temp = lv_label_create(scr);
        wea_label = lv_label_create(scr);
        precip = lv_label_create(scr);
        precip_img = lv_label_create(scr);
        weather = lv_label_create(scr);
        
        //Time
        lv_label_set_text(time, "X:XX");
        lv_obj_align(time, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_style_text_font(time, &jetbrains_mono_16,0);

        //Date
        lv_label_set_text(date, "XX/XX/XX");
        lv_obj_align(date, LV_ALIGN_TOP_LEFT, 0, 20);
        lv_obj_set_style_text_font(date, &jetbrains_mono_16,0);

        //Weather
        lv_label_set_text(weather, clear_sky);
        lv_obj_align(weather, LV_ALIGN_TOP_LEFT, 0, 40);
        lv_obj_set_style_text_font(weather, &weather_symbols,0);

        //Weather Label
        lv_label_set_text(wea_label, "LOADING");
        lv_obj_align(wea_label, LV_ALIGN_TOP_LEFT, 20, 40);
        lv_obj_set_style_text_font(wea_label, &jetbrains_mono_16,0);

        //Temperature
        lv_label_set_text(temp, "XX°F");
        lv_obj_align(temp, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_style_text_font(temp, &jetbrains_mono_16,0);

        //Precipitation Amount
        lv_label_set_text(precip,"X.XX");
        lv_obj_align(precip, LV_ALIGN_TOP_RIGHT, 0, 20);
        lv_obj_set_style_text_font(precip, &jetbrains_mono_16,0);

        //Precipitation Image
        lv_label_set_text(precip_img, precipipation);
        lv_obj_align_to(precip_img, precip,LV_ALIGN_TOP_RIGHT, -40, 0);
        lv_obj_set_style_text_font(precip_img, &weather_symbols,0);

        // Release the mutex    
        lvgl_port_unlock();
    }
}

//Updates the lvgl labels depending on the type of data
void lvgl_update(float* data){
    if (lvgl_port_lock(0)) {
        uint8_t data_len = (int)data[0];
        ESP_LOGI("LVGL","Data Length:%d",data_len);
        
        //Time and Date
        if(data_len == 6){ //{LENGTH, DAY, MONTH, YEAR, HOUR, MINUTES}
            ESP_LOGI("LVGL","Updating the Time and Date");
            //Time Change
            if (data == NULL) {
                ESP_LOGE("ERROR", "data array is NULL");
                return;
            }
            
            uint8_t hour = (((int)data[4] + 11) % 12) + 1; //conversion to 12-hour time
            snprintf(buf, buf_len, "%02d:%02d", hour, (int)data[5]);
            if((int)data[4] < 12){ //add am / pm
                strcat(buf,"AM");
            }
            else{
                strcat(buf,"PM");
            }
            lv_label_set_text(time, buf);
            
            //format date data
            uint8_t year = (int)data[3]-2000;
            snprintf(buf, buf_len, "%02d/%02d/%d", (int)data[2], (int)data[1],year);
            lv_label_set_text(date, buf);
        }
        //Weather
        else if(data_len == 4){
            ESP_LOGI("LVGL","Updating the Weather Info");
            //Temperature
            snprintf(buf,buf_len,"%02d°F",(int)data[1]);
            lv_label_set_text(temp, buf);

            //Weather & Label
            WeatherLabel current; 
            if(data[3] == 0){ //if weather code fits ranges from OpenMeteo
                current = WeatherData[0];
            }
            else if(data[3] > 0 && data[3] <=48){
                current = WeatherData[1];
            }
            else if(data[3] > 48 && data[3] <=57){
                current = WeatherData[2];
            }
            else if( (data[3] >=61 && data[3] <=67) || (data[3] >= 80 && data[3] <= 82) ){
                current = WeatherData[3];
            }
            else if( (data[3] >=71 && data[3] <=77) || (data[3] >= 85 && data[3] <= 86) ){
                current = WeatherData[4];
            }
            else if(data[3] >= 95){
                current = WeatherData[5];
            }
            else{
                ESP_LOGE("ERROR","CANT FIND WEATHER CODE");
            }
            //Weather Label
            lv_label_set_text(weather, current.font_label);
            lv_label_set_text(wea_label, current.name);

            //Precipitation Amount
            snprintf(buf,buf_len,"%.2f",data[2]);
            lv_label_set_text(precip,buf);
            
        }
        lvgl_port_unlock();
    }
}


