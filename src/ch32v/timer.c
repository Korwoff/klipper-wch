// CH32V timer support using the WCH SysTick peripheral
//
// Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_CLOCK_FREQ
#include "board/internal.h" // SysTick
#include "board/irq.h" // irq_disable
#include "board/misc.h" // timer_from_us
#include "board/timer_irq.h" // timer_dispatch_many
#include "command.h" // shutdown
#include "sched.h" // DECL_INIT

DECL_CONSTANT("CLOCK_FREQ", CONFIG_CLOCK_FREQ);

// Return the number of clock ticks for a given number of microseconds
uint32_t
timer_from_us(uint32_t us)
{
    return us * (CONFIG_CLOCK_FREQ / 1000000);
}

// Return true if time1 is before time2
uint8_t
timer_is_before(uint32_t time1, uint32_t time2)
{
    return (int32_t)(time1 - time2) < 0;
}

// Return the current time (in absolute clock ticks)
uint32_t __always_inline
timer_read_time(void)
{
#if CONFIG_MACH_CH32V20x
    // CH32V203 has 64-bit counter; read low 32 bits (little-endian)
    return (uint32_t)SysTick->CNT;
#else
    return SysTick->CNT;
#endif
}

// Activate timer dispatch as soon as possible
void
timer_kick(void)
{
    SysTick->CMP = timer_read_time() + 50;
    SysTick->SR = 0;
}

// Implement simple early-boot delay mechanism
void
udelay(uint32_t usecs)
{
    uint32_t end = timer_read_time() + timer_from_us(usecs);
    while (timer_is_before(timer_read_time(), end))
        ;
}

static uint32_t timer_repeat_until;
#define TIMER_REPEAT_TICKS timer_from_us(100)
#define TIMER_MIN_TRY_TICKS timer_from_us(2)
#define TIMER_DEFER_REPEAT_TICKS timer_from_us(5)

// Invoke timers - called from board irq code
uint32_t
timer_dispatch_many(void)
{
    uint32_t tru = timer_repeat_until;
    for (;;) {
        uint32_t next = sched_timer_dispatch();
        uint32_t now = timer_read_time();
        int32_t diff = next - now;
        if (diff > (int32_t)TIMER_MIN_TRY_TICKS)
            return next;

        if (unlikely(timer_is_before(tru, now))) {
            if (diff < (int32_t)(-timer_from_us(1000)))
                try_shutdown("Rescheduled timer in the past");
            if (sched_check_set_tasks_busy()) {
                timer_repeat_until = now + TIMER_REPEAT_TICKS;
                return now + TIMER_DEFER_REPEAT_TICKS;
            }
            timer_repeat_until = tru = now + TIMER_REPEAT_TICKS;
        }

        irq_enable();
        while (unlikely(diff > 0))
            diff = next - timer_read_time();
        irq_disable();
    }
}

void __visible __aligned(16)
SysTick_Handler(void)
{
    irq_disable();
    uint32_t next = timer_dispatch_many();
    SysTick->CMP = next;
    SysTick->SR = 0;
    irq_enable();
}

void
timer_task(void)
{
    uint32_t now = timer_read_time();
    irq_disable();
    if (timer_is_before(timer_repeat_until, now))
        timer_repeat_until = now;
    irq_enable();
}
DECL_TASK(timer_task);

void
timer_init(void)
{
    irqstatus_t flag = irq_save();

    // Configure SysTick as continuous counter with interrupt on compare
    SysTick->CTLR = 0;
    SysTick->SR = 0;
#if CONFIG_MACH_CH32V20x
    SysTick->CNT = 0;
#else
    SysTick->CNT = 0;
#endif
    SysTick->CMP = 0xFFFFFFFF;
    SysTick->CTLR = 0x0F; // STE + STIE + STCLK + (enable)

    PFIC->IPRIOR[SysTick_IRQn] = 0x40;
    NVIC_EnableIRQ(SysTick_IRQn);

    timer_kick();
    irq_restore(flag);
}
DECL_INIT(timer_init);

