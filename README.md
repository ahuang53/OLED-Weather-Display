# OLED Weather Display Project

## Project Goal
The aim of this project is to demonstrate my understanding of Real Time Operating System (RTOS) through ESP-IDF's implementation of
FreeRTOS. This was done through a practical and easy-to-understand embedded system on the ESP32 Development Board.

### Features and Description
- The program is configured for the 128x64 SSD1306 OLED I2C Display. It requires an active internet connection to function.
- Upon startup, the program will display the current time, date, and main weather statistics for my area. Then, it will update the time on the display every minute, as well as updating the weather data every hour.

### Credits
- **Open Meteo**: Weather data provided by [Open Meteo Weather Forecast API](https://open-meteo.com/).
- **ESP-IDF**: Built using the [ESP-IDF](https://github.com/espressif/esp-idf) framework.

