#include "esp_http_server.h"
#include "esp_log.h"
#include "wifi_module.h"
#include "utilities.h"
#include <ctype.h>

#include <esp_http_server.h>


static const char *TAG = "WEB_SERVER";

/* Simple HTML Form */
const char* html_page = "<html><body>"
                        "<h1>ESP32 WiFi Config</h1>"
                        "<form action='/save' method='GET'>"
                        "SSID: <input type='text' name='ssid'><br>"
                        "Pass: <input type='text' name='pass'><br>"
                        "<input type='submit' value='Save'>"
                        "</form></body></html>";

/* Handler for the root page (192.168.4.1) */
esp_err_t get_handler(httpd_req_t *req) {
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

//Simple URL decoding function
void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= '32';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= '32';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = (char)((a << 4) | b);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/* Handler to save SSID and Password */
esp_err_t save_handler(httpd_req_t *req) {
    char buf[128];

    char raw_ssid[32], raw_pass[64];
    char decoded_ssid[32], decoded_pass[64];

    // Get the query string from the URL
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK) {
        ESP_LOGI(TAG, "Query received: %s", buf);
        
        // Parse the ssid and password from the query string
        if (httpd_query_key_value(buf, "ssid", raw_ssid, sizeof(raw_ssid)) == ESP_OK &&
            httpd_query_key_value(buf, "pass", raw_pass, sizeof(raw_pass)) == ESP_OK) {
            
            // Decode them!
            url_decode(decoded_ssid, raw_ssid);
            url_decode(decoded_pass, raw_pass);

            ESP_LOGI(TAG, "Decoded SSID: %s, PASS: %s", decoded_ssid, decoded_pass);
            
            stop_provisioning_manager();
            
            // Save the DECODED versions of ssid and password
            ESP_LOGI(TAG, "Saving credentials to NVS...");
            save_wifi_credentials(decoded_ssid, decoded_pass);
            httpd_resp_send(req, "<h1>Credentials Saved! ESP32 will now restart...</h1>", HTTPD_RESP_USE_STRLEN);
            
            // Wait a moment and then restart
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
            return ESP_OK;
        }
    }
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to parse data");
    return ESP_OK;
}

/* Function to start the server */
void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Increase these values to handle modern mobile browsers
    config.max_resp_headers = 20;
    config.stack_size = 8192; // Give the server more breathing room
    config.lru_purge_enable = true; // Clean up old connections automatically

    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t uri_get = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get);

        httpd_uri_t uri_save = {
            .uri      = "/save",
            .method   = HTTP_GET,
            .handler  = save_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_save);
        
        ESP_LOGI(TAG, "Server started with increased limits.");
    }
}
