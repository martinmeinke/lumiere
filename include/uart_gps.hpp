#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#define GPS_POWER_PIN GPIO_NUM_2

// Struct to hold GPRMC data
struct GPRMCData {
  tm timeinfo;
  std::string status;
  double latitude;
  double longitude;
};

// Function to split a string by a delimiter into a vector
std::vector<std::string> split(const std::string &s, char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

// Function to parse a GPRMC GPS sentence
std::optional<GPRMCData> parseGPRMC(const std::string &gprmc) {
  GPRMCData data = {0};

  if (gprmc.find("$GPRMC") != 0 || gprmc.empty()) {
    return std::nullopt; // Not a valid GPRMC sentence
  }

  std::vector<std::string> tokens = split(gprmc, ',');
  if (tokens.size() < 12) {
    return std::nullopt; // Not enough data
  }

  // Parse time
  std::string timeStr = tokens[1];
  std::string dateStr = tokens[9];
  if (timeStr.length() >= 6 && dateStr.length() == 6) {
    data.timeinfo.tm_hour = std::stoi(timeStr.substr(0, 2));
    data.timeinfo.tm_min = std::stoi(timeStr.substr(2, 2));
    data.timeinfo.tm_sec = std::stoi(timeStr.substr(4, 2));

    // Parse date
    data.timeinfo.tm_mday = std::stoi(dateStr.substr(0, 2));
    data.timeinfo.tm_mon =
        std::stoi(dateStr.substr(2, 2)) - 1; // months since January
    data.timeinfo.tm_year =
        std::stoi(dateStr.substr(4, 2)) + 100; // years since 1900
  } else {
    return std::nullopt; // Invalid time or date format
  }

  // Parse status
  data.status = tokens[2];

  // Parse latitude and longitude
  std::string lat = tokens[3];
  std::string latDir = tokens[4];
  std::string lon = tokens[5];
  std::string lonDir = tokens[6];
  if (!lat.empty() && !lon.empty() && latDir.length() == 1 &&
      lonDir.length() == 1) {
    double latDeg = std::stod(lat.substr(0, 2));
    double latMin = std::stod(lat.substr(2));
    data.latitude = latDeg + (latMin / 60.0);
    data.latitude *= (latDir[0] == 'N') ? 1 : -1;

    double lonDeg = std::stod(lon.substr(0, 3));
    double lonMin = std::stod(lon.substr(3));
    data.longitude = lonDeg + (lonMin / 60.0);
    data.longitude *= (lonDir[0] == 'E') ? 1 : -1;
  } else {
    return std::nullopt; // Invalid latitude or longitude format
  }

  return data;
}

void set_time(GPRMCData &gps_data) {
  // Convert to epoch time (seconds since 1970-01-01 00:00:00 UTC)
  time_t t = mktime(&gps_data.timeinfo);
  // Set system time
  struct timeval now = {.tv_sec = t};
  settimeofday(&now, NULL);
}

void setup_gpio_out() {
  gpio_config_t config;
  config.pin_bit_mask = (1 << GPS_POWER_PIN);
  config.mode = GPIO_MODE_OUTPUT;
  config.pull_up_en = GPIO_PULLUP_ENABLE;
  config.pull_down_en = GPIO_PULLDOWN_DISABLE;
  config.intr_type = GPIO_INTR_DISABLE;
  gpio_config(&config);
}

void uart_init() {
  ESP_LOGI("UART", "uart_init");

  uart_config_t uart_config = {.baud_rate = 9600,
                               .data_bits = UART_DATA_8_BITS,
                               .parity = UART_PARITY_DISABLE,
                               .stop_bits = UART_STOP_BITS_1,
                               .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                               .rx_flow_ctrl_thresh = 0};

  uart_param_config(UART_NUM_1, &uart_config);

  uart_driver_install(UART_NUM_1, 1024 * 2, 0, 0, NULL, 0);
  uart_set_pin(UART_NUM_1, GPIO_NUM_4, GPIO_NUM_5, UART_PIN_NO_CHANGE,
               UART_PIN_NO_CHANGE);

  setup_gpio_out();
}

void uart_task(void *pvParameters) {
  const int uart_num = UART_NUM_1;
  uint8_t data[1024];
  while (1) {
    int len =
        uart_read_bytes(uart_num, data, sizeof(data), 20 / portTICK_PERIOD_MS);
    if (len > 0) {
      data[len] = 0; // Null-terminate the data
      ESP_LOGI("UART", "Received data: %s", data);
      auto parsed_data =
          parseGPRMC(std::string{reinterpret_cast<char *>(data)});
      if (parsed_data) {
        printTimeInfo(parsed_data->timeinfo);
        set_time(*parsed_data);
      } else {
        ESP_LOGE("UART", "Failed to parse GPRMC sentence.");
      }
    }
  }
}

void power_down_gps() {
  gpio_set_level(GPS_POWER_PIN, 0); // Set to 0 to turn off the GPS module
}

void power_up_gps() {
  gpio_set_level(GPS_POWER_PIN, 1); // Set to 1 to turn on the GPS module
}