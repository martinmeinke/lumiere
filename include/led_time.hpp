#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unordered_map>
#include <cmath>
class LedTime
{
public:
    LedTime(std::unordered_map<int, std::pair<gpio_num_t, ledc_channel_t>> &gpio_config) : m_gpio_config(gpio_config)
    {
        assert(m_gpio_config.size() == 6);
        for (const auto &pair : m_gpio_config)
        {
            const auto gpio_num_and_led_channel = pair.second;
            configure_gpio_pin_for_led(gpio_num_and_led_channel.first);
            led_pwm(gpio_num_and_led_channel.first, gpio_num_and_led_channel.second);
        }
    }

    void update(const tm &timeinfo)
    {
        const auto hour_12 = timeinfo.tm_hour % 12;
        const float hour_fraction = timeinfo.tm_min / 60.0f;
        for (int i = 0; i < 6; i++)
        {
            float intensity = 0.0f;
            if (hour_12 >= i + 1 && hour_12 < 6 + i)
            {
                intensity = 1.0f;
            }
            else
            {
                if (hour_12 == i)
                {
                    intensity = hour_fraction;
                }

                if (hour_12 == i + 6)
                {
                    intensity = 1.0f - hour_fraction;
                }
            }
            // ESP_LOGI("LED_STATUS", "LED: %i hour_fraction: %f intensity: %f", i, hour_fraction, intensity);
            //  Change duty cycle
            ledc_set_duty(LEDC_LOW_SPEED_MODE, m_gpio_config[i].second, intensityCalibration(intensity));
            ledc_update_duty(LEDC_LOW_SPEED_MODE, m_gpio_config[i].second);
        }
    }

    void demo_mode()
    {
        while (true)
        {
            tm timeinfo{0};
            for (int h = 0; 0 < 24; h++)
            {
                for (int m = 0; m < 60; m++)
                {
                    timeinfo.tm_hour = h;
                    timeinfo.tm_min = m;
                    update(timeinfo);

                    vTaskDelay(4);
                }
            }
        }
    }

private:
    int intensityCalibration(float intensity, float steepness = 3)
    {
        static constexpr int kMaxIntensity = 200;
        static constexpr int kMinIntensity = 0;

        // Calculate the interpolated value
        double normalized = (exp(steepness * intensity) - 1) / (exp(steepness) - 1);
        double y = kMinIntensity + (kMaxIntensity - kMinIntensity) * normalized;

        // Convert to integer and return
        return static_cast<int>(y);
    }

    std::unordered_map<int, std::pair<gpio_num_t, ledc_channel_t>>
        m_gpio_config;

    /**
     * @brief Configures and initializes a PWM signal for an LED on a specified GPIO pin.
     *
     * This function sets up a PWM (Pulse Width Modulation) signal for controlling an LED.
     * It initializes the PWM timer and configures the PWM channel with the specified duty cycle.
     * The frequency and resolution are predefined within the function.
     *
     * @param pin The GPIO number to which the LED is connected.
     * @param channel The LEDC channel to be used. This should be of type ledc_channel_t.
     * @param duty The duty cycle of the PWM signal. It ranges from 0 (off) to the maximum
     *             resolution defined by LEDC_TIMER_10_BIT (1023). Default is 0 (off).
     *
     * @note Ensure that the GPIO pin supports PWM functions and the channel does not conflict
     *       with other PWM channels in use.
     */
    void led_pwm(gpio_num_t pin, ledc_channel_t channel, uint32_t duty = 0)
    {
        // Setup PWM configuration
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,    // timer mode
            .duty_resolution = LEDC_TIMER_10_BIT, // resolution of PWM duty
            .timer_num = LEDC_TIMER_0,            // timer index
            .freq_hz = 1000,                      // frequency of PWM signal
            .clk_cfg = LEDC_USE_RC_FAST_CLK,      // LEDC_USE_RC_FAST_CLK survices light sleep
        };
        ledc_timer_config(&ledc_timer);

        // Configure the LED control
        ledc_channel_config_t ledc_channel = {
            .gpio_num = pin, // Change to your GPIO pin
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = channel,
            .timer_sel = LEDC_TIMER_0,
            .duty = duty,
        };
        ledc_channel_config(&ledc_channel);
    }

    void configure_gpio_pin_for_led(gpio_num_t gpio_num)
    {
        gpio_config_t config;
        config.pin_bit_mask = (1 << gpio_num);
        config.mode = GPIO_MODE_OUTPUT;
        config.pull_up_en = GPIO_PULLUP_ENABLE;
        config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        config.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&config);
    }
};