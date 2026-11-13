#include "board_power.h"

#include <stdio.h>
#include "driver/gpio.h"
#include "esp_err.h"

void od_power_init(void)
{
    // Waveshare LCD-1.46 battery latch: assert before initializing peripherals,
    // including when booting over USB, so removing USB does not cut battery power.
    // A PWR press is still required if the hardware has already cut CPU power.
    const gpio_num_t enable = GPIO_NUM_7;
    ESP_ERROR_CHECK(gpio_reset_pin(enable));
    ESP_ERROR_CHECK(gpio_set_level(enable, 1));
    ESP_ERROR_CHECK(gpio_set_direction(enable, GPIO_MODE_OUTPUT));
    puts("SCREEN_POWER keep_alive=1 gpio=7");
}
