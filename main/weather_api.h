#ifndef weather_api
#define weather_api

void wifi_setup();
void check_wifi_status();
// void api_call();
void api_get(float* api_values);
void process_json_response(const char *json_str, float* api_values);

#endif // weather_api