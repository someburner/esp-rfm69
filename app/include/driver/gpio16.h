/*
    Driver for GPIO
    Official repository: https://github.com/CHERTS/esp8266-gpio16

    Copyright (C) 2015 Mikhail Grigorev (CHERTS)

    Pin number:
    -----------
    Pin 0 = GPIO16
    Pin 1 = GPIO5
    Pin 2 = GPIO4
    Pin 3 = GPIO0
    Pin 4 = GPIO2
    Pin 5 = GPIO14
    Pin 6 = GPIO12
    Pin 7 = GPIO13
    Pin 8 = GPIO15
    Pin 9 = GPIO3
    Pin 10 = GPIO1
    Pin 11 = GPIO9
    Pin 12 = GPIO10
*/

#ifndef __GPIO16_H__
#define __GPIO16_H__
#include "gpio.h"

#define GPIO_PIN_NUM 13

#ifndef GPIO_INTERRUPT_ENABLE
#define GPIO_INTERRUPT_ENABLE 1
#endif

#define GPIO_FLOAT 0
#define GPIO_PULLUP 1
#define GPIO_PULLDOWN 2

#define GPIO_INPUT 0
#define GPIO_OUTPUT 1
#define GPIO_INT 2

/* GPIO interrupt handler */
#ifdef GPIO_INTERRUPT_ENABLE
typedef void (* gpio_intr_handler)(unsigned pin, unsigned level);
#endif

void gpio16_output_conf(void);
void gpio16_output_set(uint8 value);
void gpio16_input_conf(void);
uint8 gpio16_input_get(void);
int set_gpio_mode(unsigned pin, unsigned mode, unsigned pull);
int gpio_write(unsigned pin, unsigned level);
int gpio_read(unsigned pin);
#ifdef GPIO_INTERRUPT_ENABLE
void gpio_intr_attach(gpio_intr_handler cb);
int gpio_intr_deattach(unsigned pin);
int gpio_intr_init(unsigned pin, GPIO_INT_TYPE type);
#endif

#endif
