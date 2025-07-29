#include "ledrgb.h"

// Inicializa os pinos como saída
void init_ledrgb(uint *pin) {
    for (int i = 0; i < 3; i++) {
        gpio_init(pin[i]);
        gpio_set_dir(pin[i], GPIO_OUT);
        gpio_put(pin[i], 0); // Desliga todos os LEDs inicialmente
    }
}

// Liga todos os LEDs
void turn_on_all(uint *pin) {
    gpio_put(pin[0], 1);
    gpio_put(pin[1], 1);
    gpio_put(pin[2], 1);
}

// Desliga todos os LEDs
void turn_off_all(uint *pin) {
    gpio_put(pin[0], 0);
    gpio_put(pin[1], 0);
    gpio_put(pin[2], 0);
}

// Liga apenas o LED vermelho
void turn_on_red(uint *pin) {
    gpio_put(pin[0], 1);
    gpio_put(pin[1], 0);
    gpio_put(pin[2], 0);
}

void turn_off_red(uint *pin) {
    gpio_put(pin[0], 0);
}

// Liga apenas o LED verde
void turn_on_green(uint *pin) {
    gpio_put(pin[0], 0);
    gpio_put(pin[1], 1);
    gpio_put(pin[2], 0);
}

void turn_off_green(uint *pin) {
    gpio_put(pin[1], 0);
}

// Liga apenas o LED azul
void turn_on_blue(uint *pin) {
    gpio_put(pin[0], 0);
    gpio_put(pin[1], 0);
    gpio_put(pin[2], 1);
}

void turn_off_blue(uint *pin) {
    gpio_put(pin[2], 0);
}

// Liga vermelho + verde = amarelo
void turn_on_yellow(uint *pin) {
    gpio_put(pin[0], 1);
    gpio_put(pin[1], 1);
    gpio_put(pin[2], 0);
}

void turn_off_yellow(uint *pin) {
    gpio_put(pin[0], 0);
    gpio_put(pin[1], 0);
}

// Liga vermelho + azul = roxo
void turn_on_purple(uint *pin) {
    gpio_put(pin[0], 1);
    gpio_put(pin[1], 0);
    gpio_put(pin[2], 1);
}

void turn_off_purple(uint *pin) {
    gpio_put(pin[0], 0);
    gpio_put(pin[2], 0);
}
