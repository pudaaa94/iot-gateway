#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "mqtt_client.h"

#define WIFI_SSID "Pudici"
#define WIFI_PASS "Vedran.Angelina2"
#define SEND_INTERVAL_US (60 * 1000000LL)

static const char *TAG = "BLE_SCAN";
static esp_mqtt_client_handle_t mqtt_client;
static int64_t last_send_time = 0;
bool sending_enabled = false;

static void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://192.168.1.53:1883"
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);

    sending_enabled = true;
}

static void mqtt_publish_temp(float temp)
{
    int64_t now = esp_timer_get_time();

    if (now - last_send_time < SEND_INTERVAL_US) {
        return;
    }

    last_send_time = now;

    char msg[128];

    snprintf(msg, sizeof(msg),
        "{\"device\":\"nRFSensor\",\"temp\":%.2f}",
        temp);

    esp_mqtt_client_publish(
        mqtt_client,
        "sensors/ble",
        msg,
        0,
        1,
        0
    );
}

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI("WIFI", "Reconnect...");
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI("WIFI", "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        mqtt_init();
    }
}

static void wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        &instance_any_id
    ));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        &instance_got_ip
    ));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void parse_manufacturer_data(uint8_t *data, uint8_t len)
{
    if (len < 4) return;
    
    /* Preskoci company ID (2 bajta), uzmi temperaturu */
    int16_t temp_raw = (int16_t)((data[2] << 8) | data[3]);
    float temp = temp_raw / 100.0f;
    
    ESP_LOGI(TAG, "Temperatura: %.2f C", temp);

    if (sending_enabled)
        mqtt_publish_temp(temp);
}

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    if (event != ESP_GAP_BLE_SCAN_RESULT_EVT) return;
    if (param->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT) return;

    uint8_t *adv_data = param->scan_rst.ble_adv;
    uint8_t adv_len = param->scan_rst.adv_data_len;

    /* Provjeri ime uredjaja */
    uint8_t name_len = 0;
    uint8_t *name = esp_ble_resolve_adv_data(adv_data, 
                                              ESP_BLE_AD_TYPE_NAME_CMPL, 
                                              &name_len);
    if (!name || name_len == 0) return;
    if (strncmp((char *)name, "nRFSensor", name_len) != 0) return;

    /* Nadjen nRFSensor - parsiraj manufacturer data */
    uint8_t mfr_len = 0;
    uint8_t *mfr = esp_ble_resolve_adv_data(adv_data,
                                             ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE,
                                             &mfr_len);
    if (mfr && mfr_len >= 4) {
        parse_manufacturer_data(mfr, mfr_len);
    }
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || 
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));

    esp_ble_scan_params_t scan_params = {
        .scan_type          = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval      = 0x50,
        .scan_window        = 0x30,
        .scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE,
    };

    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scan_params));
    ESP_ERROR_CHECK(esp_ble_gap_start_scanning(0)); /* 0 = kontinualno */

    ESP_LOGI(TAG, "BLE scan pokrenut, cekam nRFSensor...");
}