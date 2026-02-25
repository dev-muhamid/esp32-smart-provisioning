#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_err.h"

#define MAX_RETRY      5

//Define access point credentials
#define ESP_WIFI_AP_SSID      "ESP32_Config_Node"
#define ESP_WIFI_AP_PASS      "12345678"
#define ESP_MAX_STA_CONN      4


void wifi_init_softap(void);
void wifi_module_init(void);
esp_err_t save_wifi_credentials(const char* ssid, const char* password);
void stop_provisioning_timer(void);

#endif