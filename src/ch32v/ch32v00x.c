// Code to setup clocks on ch32v00x
//
// Copyright (C) 2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_CLOCK_FREQ
#include "board/irq.h" // irq_save
#include "command.h" // DECL_CONSTANT_STR
#include "internal.h" // RCC


/****************************************************************
 * Clock setup
 ****************************************************************/

void
clock_setup(void)
{
    // CH32V005: PLL x2 to reach 48MHz
    // HSI 24MHz x2 = 48MHz (internal), or HSE 24MHz x2 = 48MHz (external crystal)
    FLASH->ACTLR = FLASH_ACTLR_LATENCY_1;
    if (CONFIG_CLOCK_REF_FREQ == 1) {
        // Internal HSI 24MHz
        RCC->CTLR |= RCC_HSION;
        while (!(RCC->CTLR & RCC_HSIRDY))
            ;
        RCC->CFGR0 = RCC_PLLSource_HSI_MUL2;
    } else {
        // External HSE crystal (24MHz)
        RCC->CTLR |= RCC_HSEON;
        while (!(RCC->CTLR & RCC_HSERDY))
            ;
        RCC->CFGR0 = RCC_PLLSource_HSE_MUL2;
    }
    RCC->CTLR |= RCC_PLLON;
    while (!(RCC->CTLR & RCC_PLLRDY))
        ;
    RCC->CFGR0 |= RCC_SYSCLKSource_PLLCLK;
    while ((RCC->CFGR0 & RCC_SWS) != RCC_SWS_PLL)
        ;
}


/****************************************************************
 * Peripheral clock mapping
 ****************************************************************/

// Map a peripheral address to its enable bits
struct cline
lookup_clock_line(uint32_t periph_base)
{
    if (periph_base == (uint32_t)GPIOA)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_GPIOA};
    if (periph_base == (uint32_t)GPIOB)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_GPIOB};
    if (periph_base == (uint32_t)GPIOC)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_GPIOC};
    if (periph_base == (uint32_t)GPIOD)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_GPIOD};
    if (periph_base == (uint32_t)USART1)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_USART1};
    if (periph_base == (uint32_t)USART2)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_USART2};
    if (periph_base == (uint32_t)SPI1)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_SPI1};
    if (periph_base == (uint32_t)I2C1)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_I2C1};
    if (periph_base == (uint32_t)TIM1)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_TIM1};
    if (periph_base == (uint32_t)TIM2)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_TIM2};
    if (periph_base == (uint32_t)ADC1)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_ADC1};
    if (periph_base == (uint32_t)AFIO)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_AFIO};
    return (struct cline){};
}

// Return the frequency of the given peripheral clock
uint32_t
get_pclock_frequency(uint32_t periph_base)
{
    return CONFIG_CLOCK_FREQ;
}

// Enable a GPIO peripheral clock
void
gpio_clock_enable(GPIO_TypeDef *regs)
{
    if (regs == GPIOA)
        RCC->APB2PCENR |= RCC_APB2Periph_GPIOA;
    else if (regs == GPIOB)
        RCC->APB2PCENR |= RCC_APB2Periph_GPIOB;
    else if (regs == GPIOC)
        RCC->APB2PCENR |= RCC_APB2Periph_GPIOC;
    else if (regs == GPIOD)
        RCC->APB2PCENR |= RCC_APB2Periph_GPIOD;
}

