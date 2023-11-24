#include <esp_log.h>

inline void printTimeInfo(const tm &timeinfo)
{
    ESP_LOGI("TIME", "Year: %d, Month: %d, Day: %d, Hour: %d, Minute: %d, Second: %d",
             timeinfo.tm_year + 1900, // tm_year is years since 1900
             timeinfo.tm_mon + 1,     // tm_mon is months since January (0-11)
             timeinfo.tm_mday,        // tm_mday is day of the month (1-31)
             timeinfo.tm_hour,        // tm_hour is hours since midnight (0-23)
             timeinfo.tm_min,         // tm_min is minutes after the hour (0-59)
             timeinfo.tm_sec);        // tm_sec is seconds after the minute (0-60)
}

inline bool time_is_synchronized(tm &timeinfo)
{
    time_t now = 0;
    time(&now);
    localtime_r(&now, &timeinfo);

    printTimeInfo(timeinfo);
    // time has not been set - need to fetch via WiFi
    return timeinfo.tm_year > (2022 - 1900);
}