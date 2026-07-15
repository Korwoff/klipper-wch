// CH32V GPIO pin mode configuration
//
// Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "command.h" // shutdown
#include "internal.h" // gpio_peripheral
#include "sched.h" // sched_shutdown

// Output speed: 50MHz (MODE=11).  Mirrors the STM32F1 port style where a
// single OSPEED constant is OR'd into the 4-bit cfg nibble.
#define CH32V_OSPEED 0x3

// Set the mode and extended function of a pin.
//
// The 4-bit cfg nibble for each pin on CH32V (same layout as STM32F1 CRL/CRH):
//   MODE[1:0] (bits 0-1): 00=input, 01=10MHz, 10=20MHz, 11=50MHz
//   CNF[1:0]  (bits 2-3): depends on MODE
//     input : 00=analog, 01=floating, 10=pull-up/down
//     output: 00=push-pull, 01=open-drain, 10=AF-push-pull, 11=AF-open-drain
void
gpio_peripheral(uint32_t gpio, uint32_t mode, int pullup)
{
    GPIO_TypeDef *regs = gpio_pin_to_regs(gpio);
    gpio_clock_enable(regs);

    uint32_t pos = gpio % 16, shift = (pos % 8) * 4, msk = 0xf << shift, cfg;
    if (mode == GPIO_INPUT) {
        cfg = pullup ? 0x8 : 0x4;
    } else if (mode == GPIO_OUTPUT) {
        cfg = CH32V_OSPEED;
    } else if (mode == (GPIO_OUTPUT | GPIO_OPEN_DRAIN)) {
        cfg = 0x4 | CH32V_OSPEED;
    } else if (mode == GPIO_ANALOG) {
        cfg = 0x0;
    } else {
        // Alternate function (GPIO_FUNCTION(fn))
        if (mode & GPIO_OPEN_DRAIN)
            cfg = 0xc | CH32V_OSPEED;
        else if (pullup > 0)
            // AF input pins use GPIO_INPUT mode on F1-style chips
            cfg = 0x8;
        else
            cfg = 0x8 | CH32V_OSPEED;
    }

    if (pos < 8) {
        regs->CFGLR = (regs->CFGLR & ~msk) | (cfg << shift);
    } else {
#if CONFIG_MACH_CH32V20x
        regs->CFGHR = (regs->CFGHR & ~msk) | (cfg << shift);
#else
        shutdown("Invalid GPIO pin for CH32V00x");
#endif
    }

    // Set pull-up/pull-down via ODR for input modes (F1-style: ODR bit
    // selects pull direction when CNF=10).  CH32V exposes this via the
    // BSHR (set) / BCR (reset) registers instead of STM32's BSRR.
    if (pullup > 0)
        regs->BSHR = GPIO2BIT(gpio);
    else if (pullup < 0)
        regs->BCR = GPIO2BIT(gpio);
}
