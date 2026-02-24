#ifndef BLE_PROVISIONING_H
#define BLE_PROVISIONING_H

#define GATT_WIFI_SSID_UUID 0xFF01
#define GATT_WIFI_PASS_UUID 0xFF02

void start_ble_provisioning(void);
void stop_ble_provisioning(void);

#endif
