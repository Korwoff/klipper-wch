// CH32V board startup and main entry point
//
// Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_CLOCK_FREQ
#include "board/irq.h" // irq_disable
#include "board/misc.h" // bootloader_request
#include "command.h" // DECL_CONSTANT_STR
#include "compiler.h" // __visible
#include "internal.h" // RCC
#include "sched.h" // sched_main

// Export MCU type
DECL_CONSTANT_STR("MCU", CONFIG_MCU);

// Symbols created by linker script
extern uint32_t _data_start, _data_end, _data_flash;
extern uint32_t _bss_start, _bss_end;
extern uint32_t _stack_start, _stack_end;

// Early system initialization - called from the assembly startup before main()
void __visible
SystemInit(void)
{
    clock_setup();
}

// Main entry point - called from assembly startup
void __noreturn __visible
ch32v_main(void)
{
    irq_disable();
    sched_main();
    for (;;)
        ;
}

/****************************************************************
 * Dynamic memory range
 ****************************************************************/

void *
dynmem_start(void)
{
    return &_bss_end;
}

void *
dynmem_end(void)
{
    return &_stack_start;
}


/****************************************************************
 * Bootloader request
 ****************************************************************/

// Reboot into bootloader.  On CH32V the WCH ROM bootloader is entered
// by resetting with the BOOT0 pin held high; there is no software-only
// mechanism that works on all parts.  We disable interrupts and reset.
void
bootloader_request(void)
{
    irq_disable();
    NVIC_SystemReset();
}

