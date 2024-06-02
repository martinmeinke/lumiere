#include <esp_event.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/err.h>
#include <lwip/sys.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

#include <esp_sntp.h>
#include <string>

#include "helpers.hpp"
#include "wifi_time.hpp"

static const char *TAG = "wifi_scan";

// void WifiTime::time_sync_notification_cb(const timeval *tv)
// {
//     ESP_LOGI(TAG, "Notification of a time synchronization event");
// }

// Initialize the static instance pointer
WiFiEventHandler *WiFiEventHandler::instance = nullptr;

void WiFiEventHandler::handleEvent(esp_event_base_t event_base,
                                   int32_t event_id, void *event_data) {
  ESP_LOGI(TAG, "event_base: %s, event_id: %d", event_base,
           static_cast<int>(event_id));
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    // if scanning for open wifis, we do not want to directly connect after
    // starting the station
    if (m_connect_to_preconfigured_station) {
      ESP_LOGI(TAG, "esp_wifi_connect");
      wifi_config_t wifi_config;
      if (esp_wifi_get_config(WIFI_IF_STA, &wifi_config) == ESP_OK) {
        ESP_LOGI(TAG, "Currently set SSID: %s", wifi_config.sta.ssid);
        ESP_LOGI(TAG, "Currently set pw: %s", wifi_config.sta.password);
      } else {
        ESP_LOGI(TAG, "Failed to get Wi-Fi configuration\n");
      }
      ESP_ERROR_CHECK(esp_wifi_connect());
    }
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_event_sta_disconnected_t *event =
        (wifi_event_sta_disconnected_t *)event_data;
    ESP_LOGI(TAG, "connect to the AP fail. Reason: %d", event->reason);
    if (m_retry_num < 5) {
      esp_wifi_connect();
      m_retry_num++;
      ESP_LOGI(TAG, "retry to connect to the AP");
    }
  } else if (event_base == IP_EVENT && (event_id == IP_EVENT_STA_GOT_IP ||
                                        event_id == IP_EVENT_GOT_IP6)) {
    m_retry_num = 0;
    ESP_LOGI(TAG, "Connected to AP, begin NTP sync");
    // m_wifi_time.configureSNTP();
  }
}

void WifiTime::setup_stack() {
  // Initialize NVS - required for WIFI
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // Register event handlers
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &WiFiEventHandler::eventHandlerCallback,
      NULL, &instance_got_ip));

  esp_event_handler_instance_t instance_got_ip6;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_GOT_IP6, &WiFiEventHandler::eventHandlerCallback, NULL,
      &instance_got_ip6));

  esp_event_handler_instance_t instance_any_id;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiEventHandler::eventHandlerCallback,
      NULL, &instance_any_id));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
}
void WifiTime::configureSNTP() { // Initialize SNTP
  esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  esp_sntp_setservername(0, "pool.ntp.org");
  // esp_sntp_set_time_sync_notification_cb(WifiTime::time_sync_notification_cb);

  esp_sntp_init();
}

void WifiTime::connect(const std::string ssid, const std::string pass) {
  wifi_config_t wifi_config{};

  std::copy(ssid.begin(), ssid.end(), wifi_config.sta.ssid);
  wifi_config.sta.ssid[ssid.size()] = '\0';

  std::copy(pass.begin(), pass.end(), wifi_config.sta.password);
  wifi_config.sta.password[pass.size()] = '\0';

  ESP_LOGI(TAG, "Setting wifi config to [%s/%s]",
           reinterpret_cast<char *>(wifi_config.sta.ssid),
           reinterpret_cast<char *>(wifi_config.sta.password));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  esp_wifi_start();
}

void WifiTime::scanForWifi() {
  esp_wifi_start();

  static constexpr uint16_t kMaxAps{20};
  wifi_ap_record_t ap_records[kMaxAps];

  while (true) {
    uint16_t ap_count = kMaxAps;
    memset(ap_records, 0, sizeof(ap_records));

    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);

    for (int i = 0; i < ap_count; i++) {
      std::string ssid = reinterpret_cast<char *>(ap_records[i].ssid);
      ESP_LOGI(TAG, "> Wi-Fi: %s", ssid.c_str());
    }

    for (int i = 0; i < ap_count; i++) {
      if (ap_records[i].authmode == WIFI_AUTH_OPEN) {
        std::string ssid = reinterpret_cast<char *>(ap_records[i].ssid);
        ESP_LOGI(TAG, "Found open Wi-Fi: %s", ssid.c_str());

        wifi_config_t wifi_config = {0};
        strcpy((char *)(wifi_config.sta.ssid), ssid.c_str());
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_start();
        ESP_LOGI(TAG, "Connecting to %s...", ssid.c_str());
        if (esp_wifi_connect() != ESP_OK) {
          continue;
        }

        // Wait for time to be set
        struct tm timeinfo = {0};
        int retry = 0;
        const int retry_count = 10;
        while (!time_is_synchronized(timeinfo) && ++retry < retry_count) {
          ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry,
                   retry_count);
          vTaskDelay(2000 / portTICK_PERIOD_MS);
          printTimeInfo(timeinfo);
        }

        if (retry == retry_count) {
          continue;
        }

        ESP_LOGI(TAG, "Time synchronized");
        vTaskDelete(NULL); // Cleanly deletes the task
      }
    }

    ESP_LOGE(TAG, "No open wifis!");
  }
}
