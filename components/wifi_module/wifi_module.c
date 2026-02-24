#include "freertos/FreeRTOS.h" // Always include this before other FreeRTOS headers
#include "freertos/timers.h"
#include "wifi_module.h"
#include "ble_provisioning.h"
#include "web_server.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "nvs.h"
#include <ctype.h>

static const char *TAG = "WIFI_CONN";
static TimerHandle_t ap_timeout_timer = NULL;
static bool ble_provisioning_started = false;

static void stop_softap_and_server(void);
static void ap_timer_callback(TimerHandle_t xTimer);
static void start_ap_timeout(int minutes);

// store SSID and PASSWORDS to NVS
esp_err_t save_wifi_credentials(const char* ssid, const char* password) {
    nvs_handle_t nvs_handle;
    esp_err_t err;

    //Open NVS storage in R/W mode
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    //Write SSID and Password
    err = nvs_set_str(nvs_handle, "wifi_ssid", ssid);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }
    err = nvs_set_str(nvs_handle, "wifi_pass", password);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    //commit the chnages to flash
    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    return err;
}

static void stop_softap_and_server(void) {
    ESP_LOGW("WIFI_MOD", "SoftAP timeout reached. Shutting down config mode...");
    
    // 1. Stop the Web Server
    // We'll need to store the server handle globally to stop it properly
    // For simplicity here, we stop all instances
    
    // 2. Turn off the AP part of the radio
    esp_wifi_set_mode(WIFI_MODE_NULL); 
    esp_wifi_stop();
    
    ESP_LOGI("WIFI_MOD", "Radio disabled to save power.");
}

// Callback function when timer expires
static void ap_timer_callback(TimerHandle_t xTimer) {
    stop_softap_and_server();
}

static void start_ap_timeout(int minutes) {
    ESP_LOGI("WIFI_MODE", "Config mode will timeout in %d %s,", minutes, minutes == 1 ? "Minute" : "Minutes");
    
    ap_timeout_timer = xTimerCreate("ap_timer", 
                                    pdMS_TO_TICKS(minutes * 60 * 1000), 
                                    pdFALSE, // Don't auto-reload
                                    (void*)0, 
                                    ap_timer_callback);
    
    if (ap_timeout_timer != NULL && xTimerStart(ap_timeout_timer, 0) != pdPASS)
    {
        ESP_LOGE("WIFI_MODE", "Failed to start AP timer");
    }
}

void stop_provisioning_timer(void) {
    if (ap_timeout_timer != NULL) {
        xTimerStop(ap_timeout_timer, 0);
        ESP_LOGI("WIFI_MODE", "Provisioning timer stopped.");
    }
}

// Event handler to catch WiFi events
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data) {
    static int s_retry_num = 0;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Router not found. Retrying... (%d/%d)", s_retry_num, MAX_RETRY);
        } else {
            ESP_LOGW(TAG, "Connection failed. Starting SoftAP or BLE for reconfiguration...");
            // Switch to APSTA mode so we can still try to connect while the AP is active
            wifi_init_softap(); 
            s_retry_num = 0; // Reset counter so it doesn't loop infinitely
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "WiFi connected!");

        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "Success! Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

            struct addrinfo hints = {
                .ai_family = AF_INET,
                .ai_socktype = SOCK_STREAM,
            };
            struct addrinfo* res;

            int err = getaddrinfo("google.com", "80", &hints, &res);
            if (err == 0) {
                ESP_LOGI(TAG, "DNS Lookup Success! Internet is reachable!");
                freeaddrinfo(res);
            } else {
                ESP_LOGE(TAG, "DNS Lookup Failed. Check your router's internet connection");
            }
        }
    }
}

void wifi_init_softap(void) {
    ESP_LOGI("WIFI_MODE", "Initializing SoftAP...");
    esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = ESP_WIFI_AP_SSID,
            .ssid_len = strlen(ESP_WIFI_AP_SSID),
            .password = ESP_WIFI_AP_PASS,
            .max_connection = ESP_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    if (strlen(ESP_WIFI_AP_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI_MODE", "SoftAP Started. SSID: %s", ESP_WIFI_AP_SSID);

    start_webserver();
    if (!ble_provisioning_started) {
        start_ble_provisioning();
        ble_provisioning_started = true;
        ESP_LOGI("WIFI_MODE", "BLE provisioning advertising started.");
    }

    //Start a 5-minute countdown
    start_ap_timeout(1);
}

void wifi_module_init(void) {
    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    // 2. Initialize Network Interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // esp_netif_create_default_wifi_sta(); called below at line 107

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. Register our event handler
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    //4. Check if we have saved credentials in NVS
    nvs_handle_t handle;
    char saved_ssid[32] = {0};
    char saved_pass[64] = {0};
    size_t ssid_len = sizeof(saved_ssid);
    size_t pass_len = sizeof(saved_pass);

    bool has_creds = false;

    if (nvs_open("storage", NVS_READONLY, &handle) == ESP_OK) {
        if (nvs_get_str(handle, "wifi_ssid", saved_ssid, &ssid_len) == ESP_OK && 
            nvs_get_str(handle, "wifi_pass", saved_pass, &pass_len) == ESP_OK) {
            has_creds = true;
            ESP_LOGI(TAG, "Found saved credentials in NVS");
        }
        nvs_close(handle);
    }

    if (has_creds) {
        //PATH A:: CONNECT TO SAVED WIFI
        ESP_LOGI("WIFI_MOD", "Credentials Found, Connecting to %s...", saved_ssid);
        esp_netif_create_default_wifi_sta();
        
        wifi_config_t wifi_config = {0};
        strncpy((char*)wifi_config.sta.ssid, saved_ssid, sizeof(wifi_config.sta.ssid));
        strncpy((char*)wifi_config.sta.password, saved_pass, sizeof(wifi_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "WiFi Started");
        return; 
    } else {
        //PATH B:: START SOFTAP FOR CONFIG
        ESP_LOGI("WIFI_MODE", "No Credentials Found, Starting SoftAP for configuration...");
        wifi_init_softap();
    }
}
