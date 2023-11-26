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
#include "uart_gps.hpp"
#include "wifi_time.hpp"
#include "led_time.hpp"

#include <unordered_map>

#define DEMO_MODE 0
#define CONNECT_TO_KNOWN_STATION true
#define ESP_WIFI_SSID "OmNomNom"
#define ESP_WIFI_PASS "abcdef14121990"

extern "C" void app_main()
{
    ESP_LOGI("MAIN", "starting");
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    // Timezone Berlin: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
    // TODO try to lookup via webservice or use GPS?
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();

    uart_init();
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, NULL);

    std::unordered_map<int, std::pair<gpio_num_t, ledc_channel_t>> led_gpio_channels{
        {0, std::make_pair(GPIO_NUM_13, LEDC_CHANNEL_0)},
        {1, std::make_pair(GPIO_NUM_12, LEDC_CHANNEL_1)},
        {2, std::make_pair(GPIO_NUM_14, LEDC_CHANNEL_2)},
        {3, std::make_pair(GPIO_NUM_27, LEDC_CHANNEL_3)},
        {4, std::make_pair(GPIO_NUM_26, LEDC_CHANNEL_4)},
        {5, std::make_pair(GPIO_NUM_25, LEDC_CHANNEL_5)},
    };
    LedTime led_time{led_gpio_channels};
    if (DEMO_MODE)
    {
        led_time.demo_mode();
    }

    // WifiTime wifi_time{CONNECT_TO_KNOWN_STATION};
    // wifi_time.setup_stack();
    // wifi_time.configureSNTP();

    tm timeinfo;
    while (true)
    {
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

        while (!time_is_synchronized(timeinfo))
        {
            ESP_LOGI("TIMESYNC", "Waiting for timesync");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

        led_time.update(timeinfo);

        // Configure wakeup timer for 10 seconds
        esp_sleep_enable_timer_wakeup(60 * 1000000); // 10 seconds

        // Log message (optional, for debug purposes)
        ESP_LOGI("SLEEP", "Entering light sleep for 10 seconds");
        esp_light_sleep_start();
    }
}
