#include "buzzer.h"

void init_buzzer(uint gpio_pin, float clkdiv, uint16_t period) {
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(gpio_pin);
    pwm_set_clkdiv(slice, clkdiv);
    pwm_set_wrap(slice, period);
    pwm_set_gpio_level(gpio_pin, 0); // Inicia desligado
    pwm_set_enabled(slice, true);
}

void turn_on_buzzer(uint gpio_pin, uint16_t level) {
    pwm_set_gpio_level(gpio_pin, level);
}

void turn_off_buzzer(uint gpio_pin) {
    pwm_set_gpio_level(gpio_pin, 0);
}
