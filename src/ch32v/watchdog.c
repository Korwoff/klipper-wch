// Watchdog handler on CH32V boards
//
// Copyright (C) 2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_MACH_CH32V00x
#include "internal.h" // IWDG
#include "sched.h" // DECL_TASK

// Feed the watchdog
void
watchdog_reset(void)
{
    IWDG->CTLR = 0xAAAA;
}
DECL_TASK(watchdog_reset);

// Start the watchdog (~500ms timeout)
// V00x: LSI ~128kHz -> /16 prescaler, RLDR=0xF9F -> ~500ms
// V20x: LSI ~40kHz  -> /4  prescaler, RLDR=0x0FFF -> ~410ms
void
watchdog_init(void)
{
    // Enable register access
    IWDG->CTLR = 0x5555;
#if CONFIG_MACH_CH32V00x
    // LSI ~128kHz: (0xF9F+1) * 16 / 128000 = 0.500s
    IWDG->PSCR = 2;       // Prescaler /16
    IWDG->RLDR = 0x0F9F;
#else
    // LSI ~40kHz: (0x0FFF+1) * 4 / 40000 = 0.410s
    IWDG->PSCR = 0;       // Prescaler /4
    IWDG->RLDR = 0x0FFF;
#endif
    // Start the watchdog
    IWDG->CTLR = 0xCCCC;
}
DECL_INIT(watchdog_init);

