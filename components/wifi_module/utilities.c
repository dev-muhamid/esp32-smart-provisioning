#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "utilities.h"

#define MAX_COMPONENTS 5

static const char* TAG = "UTILITIES";
static stop_action_cb_t stop_callbacks[MAX_COMPONENTS];
static int callback_count = 0;
static TimerHandle_t global_prov_timer = NULL;

static void global_timer_callback(TimerHandle_t xTimer) {
    ESP_LOGW(TAG, "Provisioning period expired, Shutting down all advertisemnets");
    for (int i = 0; i < callback_count; i++) {
        if (stop_callbacks[i] != NULL) {
            stop_callbacks[i]();
        }
    }
}

void register_stop_component(stop_action_cb_t cb) {
    if (callback_count < MAX_COMPONENTS) {
        stop_callbacks[callback_count++] = cb;
    }   
}

void start_provisioning_manager(int timeout_min) {
    if (global_prov_timer != NULL) {
        ESP_LOGI(TAG, "Provisioning timer already active; keeping existing timeout window");
        return;
    }

    global_prov_timer = xTimerCreate("ProvTimer", pdMS_TO_TICKS(timeout_min * 60000), pdFALSE, NULL, global_timer_callback);
    if (global_prov_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create provisioning timer");
    } else {
        xTimerStart(global_prov_timer, 0);
        ESP_LOGI(TAG, "Provisioning timer started for %d minutes", timeout_min);
    }
}

void stop_provisioning_manager(void) {
    if (global_prov_timer != NULL) {
        xTimerStop(global_prov_timer, 0);
        xTimerDelete(global_prov_timer, 0);
        global_prov_timer = NULL;
        ESP_LOGI(TAG, "Provisioning timer stopped");
    }
}
