idf_component_register(SRCS 
                    "wifi_module.c" 
                    "web_server.c"
                    "ble_provisioning.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_wifi esp_event nvs_flash esp_http_server bt)
