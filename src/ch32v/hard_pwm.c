// Hardware PWM support on CH32V
//
// Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_MACH_CH32V20x
#include "board/irq.h" // irq_save
#include "command.h" // shutdown
#include "gpio.h" // gpio_pwm_write
#include "internal.h" // GPIO, TIM
#include "sched.h" // sched_shutdown

#define MAX_PWM (1<<15)
DECL_CONSTANT("PWM_MAX", MAX_PWM);

struct gpio_pwm_info {
    TIM_TypeDef *timer;
    uint8_t pin, channel;
};

static const struct gpio_pwm_info pwm_regs[] = {
    {TIM1, GPIO('A', 8),  1},
    {TIM1, GPIO('A', 9),  2},
    {TIM1, GPIO('A', 10), 3},
    {TIM1, GPIO('A', 11), 4},
    {TIM2, GPIO('A', 0),  1},
    {TIM2, GPIO('A', 1),  2},
    {TIM2, GPIO('A', 2),  3},
    {TIM2, GPIO('A', 3),  4},
#if CONFIG_MACH_CH32V20x
    {TIM3, GPIO('A', 6),  1},
    {TIM3, GPIO('A', 7),  2},
    {TIM3, GPIO('B', 0),  3},
    {TIM3, GPIO('B', 1),  4},
    {TIM4, GPIO('B', 6),  1},
    {TIM4, GPIO('B', 7),  2},
    {TIM4, GPIO('B', 8),  3},
    {TIM4, GPIO('B', 9),  4},
#endif
};

#define TIM_CTLR1_CEN       ((uint16_t)0x0001)
#define TIM_SWEVGR_UG       ((uint16_t)0x0001)
#define TIM_BDTR_MOE        ((uint16_t)0x8000)

#define TIM_CCER_CC1E       ((uint16_t)0x0001)
#define TIM_CCER_CC2E       ((uint16_t)0x0010)
#define TIM_CCER_CC3E       ((uint16_t)0x0100)
#define TIM_CCER_CC4E       ((uint16_t)0x1000)

// PWM mode 1 (110) with preload and fast enable for a channel
#define CH1_PWM_FLAGS       ((uint16_t)0x006C)
#define CH2_PWM_FLAGS       ((uint16_t)0x6C00)
#define CH3_PWM_FLAGS       ((uint16_t)0x006C)
#define CH4_PWM_FLAGS       ((uint16_t)0x6C00)

struct gpio_pwm
gpio_pwm_setup(uint8_t pin, uint32_t cycle_time, uint32_t val)
{
    // Find pin in pwm_regs table
    const struct gpio_pwm_info *p = pwm_regs;
    for (;; p++) {
        if (p >= &pwm_regs[ARRAY_SIZE(pwm_regs)])
            shutdown("Not a valid PWM pin");
        if (p->pin == pin)
            break;
    }
    gpio_peripheral(p->pin, GPIO_FUNCTION(0), 0);

    // Map cycle_time to pwm clock divisor
    uint32_t pclk = get_pclock_frequency((uint32_t)p->timer);
    uint32_t pclock_div = CONFIG_CLOCK_FREQ / pclk;
    if (pclock_div > 1)
        pclock_div /= 2; // Timers run at twice the normal pclock frequency
    uint32_t pcycle_time = cycle_time / pclock_div;

    // Convert requested cycle time to actual hwpwm ticks/prescaler
    uint32_t hwpwm_ticks = pcycle_time, prescaler = 1, shift = 0;
    while (hwpwm_ticks > UINT16_MAX) {
        shift += 1;
        hwpwm_ticks = (pcycle_time + (1 << (shift-1))) >> shift;
        prescaler = 1 << shift;
    }
    if (prescaler > UINT16_MAX + 1) {
        prescaler = UINT16_MAX + 1;
        hwpwm_ticks = UINT16_MAX;
    }
    if (hwpwm_ticks < 2)
        hwpwm_ticks = 2;

    // Enable requested pwm hardware block
    if (!is_enabled_pclock((uint32_t) p->timer)) {
        enable_pclock((uint32_t) p->timer);
    }
    if (p->timer->CTLR1 & TIM_CTLR1_CEN) {
        if (p->timer->PSC != (uint16_t) (prescaler - 1)) {
            shutdown("PWM already programmed at different speed");
        }
        if (p->timer->ATRLR != (uint16_t) (hwpwm_ticks - 1)) {
            shutdown("PWM already programmed with different pulse duration");
        }
    } else {
        p->timer->PSC = prescaler - 1;
        p->timer->ATRLR = hwpwm_ticks - 1;
        p->timer->SWEVGR = TIM_SWEVGR_UG;
    }

    // Enable requested channel of hardware pwm block
    struct gpio_pwm channel;
    channel.hwpwm_ticks = hwpwm_ticks;
    switch (p->channel) {
        case 1: {
            channel.reg = (void*) &p->timer->CH1CVR;
            p->timer->CCER &= ~TIM_CCER_CC1E;
            p->timer->CHCTLR1 &= ~(uint16_t)0x00FF;
            p->timer->CHCTLR1 |= CH1_PWM_FLAGS;
            gpio_pwm_write(channel, val);
            p->timer->CCER |= TIM_CCER_CC1E;
            break;
        }
        case 2: {
            channel.reg = (void*) &p->timer->CH2CVR;
            p->timer->CCER &= ~TIM_CCER_CC2E;
            p->timer->CHCTLR1 &= ~(uint16_t)0xFF00;
            p->timer->CHCTLR1 |= CH2_PWM_FLAGS;
            gpio_pwm_write(channel, val);
            p->timer->CCER |= TIM_CCER_CC2E;
            break;
        }
        case 3: {
            channel.reg = (void*) &p->timer->CH3CVR;
            p->timer->CCER &= ~TIM_CCER_CC3E;
            p->timer->CHCTLR2 &= ~(uint16_t)0x00FF;
            p->timer->CHCTLR2 |= CH3_PWM_FLAGS;
            gpio_pwm_write(channel, val);
            p->timer->CCER |= TIM_CCER_CC3E;
            break;
        }
        case 4: {
            channel.reg = (void*) &p->timer->CH4CVR;
            p->timer->CCER &= ~TIM_CCER_CC4E;
            p->timer->CHCTLR2 &= ~(uint16_t)0xFF00;
            p->timer->CHCTLR2 |= CH4_PWM_FLAGS;
            gpio_pwm_write(channel, val);
            p->timer->CCER |= TIM_CCER_CC4E;
            break;
        }
        default:
            shutdown("Invalid PWM channel");
    }

    // Enable PWM output
    p->timer->CTLR1 |= TIM_CTLR1_CEN;

    // Advanced timers need MOE enabled.  On standard timers this is a
    // write to reserved memory, but that seems harmless in practice.
    p->timer->BDTR = TIM_BDTR_MOE;

    return channel;
}

void
gpio_pwm_write(struct gpio_pwm g, uint32_t val)
{
    uint32_t r = DIV_ROUND_CLOSEST(val * g.hwpwm_ticks, MAX_PWM);
    *(volatile uint32_t *)g.reg = r;
}

