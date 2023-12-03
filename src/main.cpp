#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_sleep.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <time.h>

#include "helpers.hpp"
#include "led_time.hpp"
#include "light_sensor.hpp"
#include "uart_gps.hpp"
#include "wifi_time.hpp"

#include <unordered_map>

#define DEMO_MODE 0
#define CONNECT_TO_KNOWN_STATION true
#define ESP_WIFI_SSID "OmNomNom"
#define ESP_WIFI_PASS "abcdef14121990"

void check_battery_voltage_and_sleep(LedTime* led_time=nullptr) {
  static constexpr float kAdcRefVoltage = 3.3;
  static constexpr float kVoltageDividerFactor = 2.0;
  const auto adc_value_battery = read_adc_value(ADC1_CHANNEL_7);
  const auto battery_voltage =
      adc_value_battery * kAdcRefVoltage * kVoltageDividerFactor;
  ESP_LOGI("BATTERY", "Voltage: %f\n", battery_voltage);

  static constexpr auto kSleepBv = 3.0;
  if (battery_voltage < kSleepBv) {
    power_down_gps();
    if(led_time != nullptr)
    {
      led_time->turn_off();
    }
    esp_sleep_enable_timer_wakeup(100000000); // 100 seconds in microseconds
    // Enter deep sleep mode
    esp_deep_sleep_start();
  }
}

extern "C" void app_main() {
  ESP_LOGI("MAIN", "starting");
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  // Timezone Berlin:
  // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
  // TODO try to lookup via webservice or use GPS?
  setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
  tzset();

  init_adc(ADC1_CHANNEL_6);
  init_adc(ADC1_CHANNEL_7);
  uart_init();

  check_battery_voltage_and_sleep();

  power_up_gps();
  xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, NULL);

  std::unordered_map<int, std::pair<gpio_num_t, ledc_channel_t>>
      led_gpio_channels{
          {0, std::make_pair(GPIO_NUM_13, LEDC_CHANNEL_0)},
          {1, std::make_pair(GPIO_NUM_12, LEDC_CHANNEL_1)},
          {2, std::make_pair(GPIO_NUM_14, LEDC_CHANNEL_2)},
          {3, std::make_pair(GPIO_NUM_27, LEDC_CHANNEL_3)},
          {4, std::make_pair(GPIO_NUM_26, LEDC_CHANNEL_4)},
          {5, std::make_pair(GPIO_NUM_25, LEDC_CHANNEL_5)},
      };
  LedTime led_time{led_gpio_channels};
  if (DEMO_MODE) {
    led_time.demo_mode();
  }

  // WifiTime wifi_time{CONNECT_TO_KNOWN_STATION};
  // wifi_time.setup_stack();
  // wifi_time.configureSNTP();

  tm timeinfo;
  while (true) {
    const auto adc_value_light_sensor = read_adc_value(ADC1_CHANNEL_6);
    ESP_LOGI("LIGHT", "ADC Value: %f\n", adc_value_light_sensor);

    check_battery_voltage_and_sleep(&led_time);

    //         if (!time_is_synchronized(timeinfo))
    //         {
    //             if (!wifi_time.isConnected())
    //             {
    // #ifdef ESP_WIFI_SSID
    //                 wifi_time.connect(ESP_WIFI_SSID, ESP_WIFI_PASS);
    // #else
    //                 wifi_time.scanForWifi();
    // #endif
    //             }
    //         }

    while (!time_is_synchronized(timeinfo)) {
      power_up_gps();
      ESP_LOGI("TIMESYNC", "Waiting for timesync");
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    power_down_gps();

    led_time.update(timeinfo, adc_value_light_sensor);

    // Configure wakeup timer for 10 seconds
    esp_sleep_enable_timer_wakeup(1 * 1000000); // 10 seconds

    // Log message (optional, for debug purposes)
    ESP_LOGI("SLEEP", "Entering light sleep for 10 seconds");
    esp_light_sleep_start();
  }
}
