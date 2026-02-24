#include <stdio.h>
#include "wifi_module.h"
// #include "ble_provisioning.h"
#include "esp_log.h"

// main/main.c

void app_main(void) {
    ESP_LOGI("MAIN", "Starting Provisioning System...");

    // 1. Initialize the Wi-Fi stack and NVS
    wifi_module_init(); 
    // start_ble_provisioning();

    // // 2. Start the Access Point so the phone can connect
    // wifi_init_softap(); //using for AP creation in case of failure conn in wifi_module.c in function `wifi_module_init()`
}