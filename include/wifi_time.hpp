#include <esp_event.h>
#include <esp_wifi.h>
#include <memory>

class WifiTime;

class WiFiEventHandler {
public:
  WiFiEventHandler(WifiTime &wifi_time, bool connect_to_preconfigured_station)
      : m_wifi_time(wifi_time),
        m_connect_to_preconfigured_station(connect_to_preconfigured_station) {
    // Store 'this' instance for the static callback
    instance = this;
  }

  void handleEvent(esp_event_base_t event_base, int32_t event_id,
                   void *event_data);

  static void eventHandlerCallback(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data) {
    // Forward the call to the instance's member function
    if (instance) {
      instance->handleEvent(event_base, event_id, event_data);
    }
  }

  static WiFiEventHandler *instance;
  WifiTime &m_wifi_time;
  int m_retry_num = 0;
  bool m_connect_to_preconfigured_station = false;
};

class WifiTime {
public:
  WifiTime(bool connect_to_preconfigured_station) {
    handler = std::make_unique<WiFiEventHandler>(
        *this, connect_to_preconfigured_station);
  }
  // void time_sync_notification_cb(const timeval *tv);

  void wifi_event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

  void scanForWifi();
  void connect(const std::string ssid, const std::string pass);
  void setup_stack();
  void configureSNTP();
  bool isConnected() const {
    wifi_ap_record_t ap_info;
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
  }

private:
  std::unique_ptr<WiFiEventHandler> handler;
};