#ifndef BUZZER_H
#define BUZZER_H

#include "pico/stdlib.h"
#include "hardware/pwm.h"

// Inicializa o pino como saída PWM e configura frequência
void init_buzzer(uint gpio_pin, float clkdiv, uint16_t period);

// Liga o buzzer (define nível do PWM)
void turn_on_buzzer(uint gpio_pin, uint16_t level);

// Desliga o buzzer (nível PWM = 0)
void turn_off_buzzer(uint gpio_pin);

#endif
