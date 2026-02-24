#include <assert.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "host/ble_att.h"
#include "host/ble_hs.h"
#include "host/ble_hs_mbuf.h"
#include "host/util/util.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "nimble/nimble_port.h"
#include "ble_provisioning.h"
#include "wifi_module.h"

static const char *TAG = "BLE_VISION";
static char ble_ssid[32];
static char ble_pass[64];
static uint8_t ble_addr_type;

static void ble_app_advertise(void);
static void nimble_host_task(void *param);
static int ble_gap_event(struct ble_gap_event *event, void *arg);
static int gatt_copy_value(char *dst, size_t dst_len, struct ble_gatt_access_ctxt *ctxt);
static int gatt_svr_access_wifi(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

static const ble_uuid128_t gatt_svr_svc_uuid =
    BLE_UUID128_INIT(0x2d, 0x71, 0xa1, 0x20, 0x53, 0x75, 0x49, 0x73,
                     0xad, 0xc4, 0xbc, 0x1d, 0x8d, 0x5d, 0xf6, 0x19);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { 
            {
                .uuid = BLE_UUID16_DECLARE(GATT_WIFI_SSID_UUID),
                .access_cb = gatt_svr_access_wifi,
                .flags = BLE_GATT_CHR_F_WRITE,
            }, {
                .uuid = BLE_UUID16_DECLARE(GATT_WIFI_PASS_UUID),
                .access_cb = gatt_svr_access_wifi,
                .flags = BLE_GATT_CHR_F_WRITE,
            }, {
                0, //No More Characterstics
            }
        },
    },
    {
        0, //No More Services 
    },
};

static int gatt_svr_access_wifi(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (uuid == GATT_WIFI_SSID_UUID) {
            int rc = gatt_copy_value(ble_ssid, sizeof(ble_ssid), ctxt);
            if (rc != 0) {
                return rc;
            }
            ESP_LOGI(TAG, "Received SSID via BLE: %s", ble_ssid);
        }
        if (uuid == GATT_WIFI_PASS_UUID) {
            int rc = gatt_copy_value(ble_pass, sizeof(ble_pass), ctxt);
            if (rc != 0) {
                return rc;
            }
            ESP_LOGI(TAG, "Received PASS via BLE: %s", ble_pass);

            if (strlen(ble_ssid) > 0 && strlen(ble_pass) > 0) {
                ESP_LOGI(TAG, "Saving credentials received via BLE...");
                save_wifi_credentials(ble_ssid, ble_pass); //save wifi_credentials to NVS
                stop_provisioning_timer();
                vTaskDelay(pdMS_TO_TICKS(2000));
                esp_restart();
            }
        }
    }
    return 0;
}

static int gatt_copy_value(char *dst, size_t dst_len, struct ble_gatt_access_ctxt *ctxt) {
    uint16_t len = 0;
    int rc;

    if (dst == NULL || ctxt == NULL || ctxt->om == NULL || dst_len < 2) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    memset(dst, 0, dst_len);
    rc = ble_hs_mbuf_to_flat(ctxt->om, dst, (uint16_t)(dst_len - 1), &len);
    if (rc != 0) {
        return (rc == BLE_HS_EMSGSIZE) ? BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN : BLE_ATT_ERR_UNLIKELY;
    }

    if (len == 0) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    dst[len] = '\0';
    return 0;
}

static void nimble_host_task(void *param) {
    (void)param;
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_app_on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to infer BLE address type: %d", rc);
        return;
    }
    ble_app_advertise();
}


void start_ble_provisioning(void) {
    ESP_ERROR_CHECK(nimble_port_init());

    // Set the sync callback
    ble_hs_cfg.sync_cb = ble_app_on_sync;

    // Register GAP and GATT services
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(gatt_svr_svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    assert(rc == 0);

    // set device name
    rc = ble_svc_gap_device_name_set("ESP32-BLE-VISION");
    assert(rc == 0);

    // Start NimBLE task
    nimble_port_freertos_init(nimble_host_task);
}

void stop_ble_provisioning(void) {
    ble_shutdown_requested = true;

    int rc = ble_gap_adv_stop();
    if (rc != 0) {
        ESP_LOGW(TAG, "BLE advertising stop returned: %d", rc);
    } else {
        ESP_LOGI(TAG, "BLE advertising stopped.");
    }
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_DISCONNECT:
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ble_app_advertise();
        return 0;
    default:
        return 0;
    }
}

static void ble_app_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields adv_fields;
    int rc;

    memset(&adv_fields, 0, sizeof(adv_fields));

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    adv_fields.appearance = 0x0080;
    adv_fields.appearance_is_present = 1;

    adv_fields.name = (uint8_t *)"ESP_BLE_VISION";
    adv_fields.name_len = strlen("ESP_BLE_VISION");
    adv_fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting advertisement fields: %d", rc);
        return;
    }

    //Define Advertising parameters
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; //Unidirected Connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; //General Discovery Mode

    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertising: %d", rc);
    } else {
        ESP_LOGI(TAG, "BLE Advertising Started....!"); 
    }
}
