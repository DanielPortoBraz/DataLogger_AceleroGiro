#ifndef LEDRGB_H
#define LEDRGB_H

#include <stdlib.h>
#include "pico/stdlib.h"

void init_ledrgb(uint *pin);
void turn_on_all(uint *pin);
void turn_off_all(uint *pin);
void turn_on_red(uint *pin);
void turn_off_red(uint *pin);
void turn_on_green(uint *pin);
void turn_off_green(uint *pin);
void turn_on_blue(uint *pin);
void turn_off_blue(uint *pin);
void turn_on_yellow(uint *pin);
void turn_off_yellow(uint *pin);
void turn_on_purple(uint *pin);
void turn_off_purple(uint *pin);

#endif