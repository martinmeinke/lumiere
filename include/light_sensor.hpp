#include "driver/adc.h"
#include "esp_adc_cal.h"

void init_adc(const adc1_channel_t adc_channel) {
    adc1_config_width(ADC_WIDTH_BIT_12); // Configure the ADC resolution
    adc1_config_channel_atten(adc_channel, ADC_ATTEN_DB_11); // Replace ADC1_CHANNEL_6 with your channel
}

float read_adc_value(const adc1_channel_t adc_channel) {
    int adc_value = adc1_get_raw(adc_channel); // Replace ADC1_CHANNEL_6 with your channel
    return static_cast<float>(adc_value) / pow(2, ADC_WIDTH_BIT_12);
}