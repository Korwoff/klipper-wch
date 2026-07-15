// CH32V RISC-V interrupt enable/disable helpers
//
// Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "board/internal.h" // __enable_irq/__disable_irq
#include "board/irq.h" // irqstatus_t
#include "sched.h" // DECL_SHUTDOWN

void
irq_disable(void)
{
    __disable_irq();
}

void
irq_enable(void)
{
    __enable_irq();
}

irqstatus_t
irq_save(void)
{
    irqstatus_t flag;
    uint32_t mstatus;
    __asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    flag = mstatus & 0x88;
    irq_disable();
    return flag;
}

void
irq_restore(irqstatus_t flag)
{
    uint32_t mstatus;
    __asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    mstatus = (mstatus & ~0x88) | (flag & 0x88);
    __asm volatile("csrw mstatus, %0" : : "r"(mstatus));
}

void
irq_wait(void)
{
    irq_enable();
    __asm volatile("wfi" ::: "memory");
    irq_disable();
}

void
irq_poll(void)
{
}

// Clear the active irq if a shutdown happened in an irq handler
void
clear_active_irq(void)
{
    // On RISC-V there is no simple active-irq register to clear.
    // Returning from the handler will naturally clear it.
}
DECL_SHUTDOWN(clear_active_irq);

