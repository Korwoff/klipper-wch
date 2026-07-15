// Code to setup clocks on ch32v20x
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

// CH32V20x SDK headers omit the FLASH_ACTLR latency constants, so define them
// locally.  At 144 MHz two wait states are required.
#ifndef FLASH_ACTLR_LATENCY_0
  #define FLASH_ACTLR_LATENCY_0          ((uint8_t)0x00)
  #define FLASH_ACTLR_LATENCY_1          ((uint8_t)0x01)
  #define FLASH_ACTLR_LATENCY_2          ((uint8_t)0x02)
#endif

void
clock_setup(void)
{
    // CH32V203: PLL to reach CONFIG_CLOCK_FREQ
    // HSE 8MHz x18 = 144MHz, HSE 24MHz x6 = 144MHz, HSI 8MHz/2 x18 = 72MHz
    FLASH->ACTLR = FLASH_ACTLR_LATENCY_2;
    if (CONFIG_CLOCK_REF_FREQ == 1) {
        // Internal HSI 8MHz / 2 = 4MHz PLL input
        RCC->CTLR |= RCC_HSION;
        while (!(RCC->CTLR & RCC_HSIRDY))
            ;
        uint32_t mul = CONFIG_CLOCK_FREQ / (8000000 / 2);
        uint32_t pllmul = ((mul == 18) ? 15 : (mul - 2)) << 18;
        RCC->CFGR0 = (RCC->CFGR0 & ~(RCC_PLLSRC | RCC_PLLMULL))
                     | RCC_PLLSource_HSI_Div2 | pllmul;
    } else {
        // External HSE crystal
        RCC->CTLR |= RCC_HSEON;
        while (!(RCC->CTLR & RCC_HSERDY))
            ;
        uint32_t mul = CONFIG_CLOCK_FREQ / CONFIG_CLOCK_REF_FREQ;
        uint32_t pllmul = ((mul == 18) ? 15 : (mul - 2)) << 18;
        RCC->CFGR0 = (RCC->CFGR0 & ~(RCC_PLLSRC | RCC_PLLMULL))
                     | RCC_PLLSource_HSE_Div1 | pllmul;
    }
    RCC->CTLR |= RCC_PLLON;
    while (!(RCC->CTLR & RCC_PLLRDY))
        ;
    RCC->CFGR0 |= RCC_SYSCLKSource_PLLCLK;
    while ((RCC->CFGR0 & RCC_SWS) != RCC_SWS_PLL)
        ;
    // APB1 = HCLK/2
    RCC->CFGR0 |= RCC_HCLK_Div2;
    // ADC clock = PCLK2/8 (kept within ADC operating range)
    RCC->CFGR0 = (RCC->CFGR0 & ~RCC_ADCPRE) | RCC_ADCPRE_DIV8;
#if CONFIG_USB
    // USB clock = PLL/3 = 48MHz (requires 144MHz PLL)
    if (CONFIG_CLOCK_FREQ == 144000000)
        RCC->CFGR0 = (RCC->CFGR0 & ~((uint32_t)3 << 22)) | ((uint32_t)2 << 22);
#endif
}


/****************************************************************
 * Peripheral clock mapping
 ****************************************************************/

// Return the frequency of the given peripheral clock
uint32_t
get_pclock_frequency(uint32_t periph_base)
{
    // APB1 peripherals run at half HCLK when APB1 prescaler is DIV2
    if (periph_base == (uint32_t)USART2 || periph_base == (uint32_t)USART3
        || periph_base == (uint32_t)UART4 || periph_base == (uint32_t)SPI2
        || periph_base == (uint32_t)I2C1 || periph_base == (uint32_t)I2C2
        || periph_base == (uint32_t)CAN1 || periph_base == (uint32_t)TIM2
        || periph_base == (uint32_t)TIM3 || periph_base == (uint32_t)TIM4
        || periph_base == (uint32_t)TIM5)
        return CONFIG_CLOCK_FREQ / 2;
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
    else if (regs == GPIOE)
        RCC->APB2PCENR |= RCC_APB2Periph_GPIOE;
}

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
    if (periph_base == (uint32_t)GPIOE)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_GPIOE};
    if (periph_base == (uint32_t)USART1)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_USART1};
    if (periph_base == (uint32_t)USART2)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_USART2};
    if (periph_base == (uint32_t)USART3)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_USART3};
    if (periph_base == (uint32_t)UART4)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_UART4};
    if (periph_base == (uint32_t)SPI1)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_SPI1};
    if (periph_base == (uint32_t)SPI2)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_SPI2};
    if (periph_base == (uint32_t)I2C1)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_I2C1};
    if (periph_base == (uint32_t)I2C2)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_I2C2};
    if (periph_base == (uint32_t)CAN1)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_CAN1};
    if (periph_base == (uint32_t)TIM1)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_TIM1};
    if (periph_base == (uint32_t)TIM2)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_TIM2};
    if (periph_base == (uint32_t)TIM3)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_TIM3};
    if (periph_base == (uint32_t)TIM4)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_TIM4};
    if (periph_base == (uint32_t)TIM5)
        return (struct cline){&RCC->APB1PCENR, RCC_APB1Periph_TIM5};
    if (periph_base == (uint32_t)ADC1)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_ADC1};
    if (periph_base == (uint32_t)ADC2)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_ADC2};
    if (periph_base == (uint32_t)AFIO)
        return (struct cline){&RCC->APB2PCENR, RCC_APB2Periph_AFIO};
    if (periph_base == (uint32_t)USBFS_BASE)
        return (struct cline){&RCC->AHBPCENR, RCC_USBFS};
    return (struct cline){};
}

