// Local definitions for CH32V code
//
// Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#ifndef __CH32V_INTERNAL_H
#define __CH32V_INTERNAL_H

#include "autoconf.h"

#if CONFIG_MACH_CH32V00x
#include "ch32v00x.h"
#elif CONFIG_MACH_CH32V20x
#include "ch32v20x.h"
#endif

// gpio.c
GPIO_TypeDef *gpio_pin_to_regs(uint32_t pin);
#define GPIO(PORT, NUM) (((PORT)-'A') * 16 + (NUM))
#define GPIO2PORT(PIN) ((PIN) / 16)
#define GPIO2BIT(PIN) (1<<((PIN) % 16))

// gpioperiph.c
#define GPIO_INPUT 0
#define GPIO_OUTPUT 1
#define GPIO_OPEN_DRAIN 0x100
#define GPIO_HIGH_SPEED 0x200
#define GPIO_FUNCTION(fn) (2 | ((fn) << 4))
#define GPIO_ANALOG 3
void gpio_peripheral(uint32_t gpio, uint32_t mode, int pullup);

// ch32v00x.c / ch32v20x.c
void clock_setup(void);

// clockline.c
struct cline {
    volatile uint32_t *en;
    uint32_t bit;
};
struct cline lookup_clock_line(uint32_t periph_base);
void enable_pclock(uint32_t periph_base);
int is_enabled_pclock(uint32_t periph_base);
uint32_t get_pclock_frequency(uint32_t periph_base);
void gpio_clock_enable(GPIO_TypeDef *regs);

// timer.c
void udelay(uint32_t usecs);

#endif // internal.h

