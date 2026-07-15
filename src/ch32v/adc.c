// ADC functions on CH32V
//
// Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "board/irq.h" // irq_save
#include "board/misc.h" // timer_from_us, udelay
#include "command.h" // shutdown
#include "compiler.h" // ARRAY_SIZE
#include "gpio.h" // gpio_adc_setup
#include "internal.h" // GPIO, ADC
#include "sched.h" // sched_shutdown

DECL_CONSTANT("ADC_MAX", 4095);

#if CONFIG_MACH_CH32V20x
#define ADC_TEMPERATURE_PIN 0xfe
DECL_ENUMERATION("pin", "ADC_TEMPERATURE", ADC_TEMPERATURE_PIN);
#endif

static const uint8_t adc_pins[] = {
    GPIO('A', 0), GPIO('A', 1), GPIO('A', 2), GPIO('A', 3),
    GPIO('A', 4), GPIO('A', 5), GPIO('A', 6), GPIO('A', 7),
#if CONFIG_MACH_CH32V20x
    GPIO('B', 0), GPIO('B', 1),
    GPIO('C', 0), GPIO('C', 1), GPIO('C', 2), GPIO('C', 3),
    GPIO('C', 4), GPIO('C', 5),
    ADC_TEMPERATURE_PIN,
#endif
};

// Perform calibration
static void
adc_calibrate(ADC_TypeDef *adc)
{
    adc->CTLR2 = ADC_ADON;
    udelay(10);
    adc->CTLR2 = ADC_ADON | ADC_RSTCAL;
    while (adc->CTLR2 & ADC_RSTCAL)
        ;
    adc->CTLR2 = ADC_ADON | ADC_CAL;
    while (adc->CTLR2 & ADC_CAL)
        ;
}

struct gpio_adc
gpio_adc_setup(uint32_t pin)
{
    // Find pin in adc_pins table
    int chan;
    for (chan=0; ; chan++) {
        if (chan >= ARRAY_SIZE(adc_pins))
            shutdown("Not a valid ADC pin");
        if (adc_pins[chan] == pin)
            break;
    }

    ADC_TypeDef *adc = ADC1;

    // Enable the ADC
    if (!is_enabled_pclock((uint32_t)adc)) {
        enable_pclock((uint32_t)adc);
        adc_calibrate(adc);
        // Max sample time (239.5 cycles) for stable conversions on all channels
        adc->SAMPTR1 = 0x00FFFFFF;
        adc->SAMPTR2 = 0x3FFFFFFF;
        adc->CTLR2 = ADC_ADON;
    }

    if (0) {
#if CONFIG_MACH_CH32V20x
    } else if (pin == ADC_TEMPERATURE_PIN) {
        adc->CTLR2 |= ADC_TSVREFE;
#endif
    } else {
        gpio_peripheral(pin, GPIO_ANALOG, 0);
    }

    return (struct gpio_adc){ .adc = adc, .chan = chan };
}

// Try to sample a value. Returns zero if sample ready, otherwise
// returns the number of clock ticks the caller should wait before
// retrying this function.
uint32_t
gpio_adc_sample(struct gpio_adc g)
{
    ADC_TypeDef *adc = g.adc;
    uint32_t sr = adc->STATR;
    if (sr & ADC_STRT) {
        if (!(sr & ADC_EOC) || adc->RSQR3 != (uint32_t)g.chan)
            // Conversion still in progress or busy on another channel
            goto need_delay;
        // Conversion ready
        return 0;
    }
    // Start sample
    adc->RSQR3 = g.chan;
    adc->CTLR2 = ADC_ADON | ADC_SWSTART;

need_delay:
    return timer_from_us(20);
}

// Read a value; use only after gpio_adc_sample() returns zero
uint16_t
gpio_adc_read(struct gpio_adc g)
{
    ADC_TypeDef *adc = g.adc;
    adc->STATR = ~ADC_STRT;
    return (uint16_t)adc->RDATAR;
}

// Cancel a sample that may have been started with gpio_adc_sample()
void
gpio_adc_cancel_sample(struct gpio_adc g)
{
    ADC_TypeDef *adc = g.adc;
    irqstatus_t flag = irq_save();
    if ((adc->STATR & ADC_STRT) && adc->RSQR3 == (uint32_t)g.chan)
        gpio_adc_read(g);
    irq_restore(flag);
}

