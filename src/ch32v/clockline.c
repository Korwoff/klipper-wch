// CH32V peripheral clock enable support
//
// Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_CLOCK_FREQ
#include "board/irq.h" // irq_save
#include "command.h" // DECL_CONSTANT_STR
#include "internal.h" // RCC, GPIO

#define FREQ_PERIPH CONFIG_CLOCK_FREQ

// Enable a peripheral clock
void
enable_pclock(uint32_t periph_base)
{
    struct cline cl = lookup_clock_line(periph_base);
    if (!cl.en)
        return;
    irqstatus_t flag = irq_save();
    *cl.en |= cl.bit;
    *cl.en; // Pause to ensure peripheral is enabled
    irq_restore(flag);
}

// Check if a peripheral clock has been enabled
int
is_enabled_pclock(uint32_t periph_base)
{
    struct cline cl = lookup_clock_line(periph_base);
    if (!cl.en)
        return 0;
    return *cl.en & cl.bit;
}

