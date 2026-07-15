/* CH32V RISC-V startup and vector table */
/*
 * Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
 *
 * This file may be distributed under the terms of the GNU GPLv3 license.
 */

#include "autoconf.h"

    .section .init, "ax", @progbits
    .globl _start
    .align 2
_start:
    .option norvc
    j       handle_reset
    .word   0
    .word   NMI_Handler                  /* NMI Handler */
    .word   HardFault_Handler            /* Hard Fault Handler */
    .word   0
    .word   0
    .word   0
    .word   0
    .word   0
    .word   0
    .word   0
    .word   0
    .word   SysTick_Handler              /* SysTick Handler */
    .word   0
    .word   SW_Handler                   /* SW Handler */
    .word   0
    /* External Interrupts */
    .word   WWDG_IRQHandler
    .word   PVD_IRQHandler
#if CONFIG_MACH_CH32V20x
    .word   TAMPER_IRQHandler
    .word   RTC_IRQHandler
#endif
    .word   FLASH_IRQHandler
    .word   RCC_IRQHandler
#if CONFIG_MACH_CH32V20x
    .word   EXTI0_IRQHandler
    .word   EXTI1_IRQHandler
    .word   EXTI2_IRQHandler
    .word   EXTI3_IRQHandler
    .word   EXTI4_IRQHandler
#else
    .word   EXTI7_0_IRQHandler
    .word   AWU_IRQHandler
#endif
    .word   DMA1_Channel1_IRQHandler
    .word   DMA1_Channel2_IRQHandler
    .word   DMA1_Channel3_IRQHandler
    .word   DMA1_Channel4_IRQHandler
    .word   DMA1_Channel5_IRQHandler
    .word   DMA1_Channel6_IRQHandler
    .word   DMA1_Channel7_IRQHandler
    .word   ADC1_IRQHandler
#if CONFIG_MACH_CH32V20x
    .word   USB_HP_CAN1_TX_IRQHandler
    .word   USB_LP_CAN1_RX0_IRQHandler
    .word   CAN1_RX1_IRQHandler
    .word   CAN1_SCE_IRQHandler
    .word   EXTI9_5_IRQHandler
    .word   TIM1_BRK_IRQHandler
    .word   TIM1_UP_IRQHandler
    .word   TIM1_TRG_COM_IRQHandler
    .word   TIM1_CC_IRQHandler
    .word   TIM2_IRQHandler
    .word   TIM3_IRQHandler
    .word   TIM4_IRQHandler
    .word   I2C1_EV_IRQHandler
    .word   I2C1_ER_IRQHandler
    .word   I2C2_EV_IRQHandler
    .word   I2C2_ER_IRQHandler
    .word   SPI1_IRQHandler
    .word   SPI2_IRQHandler
    .word   USART1_IRQHandler
    .word   USART2_IRQHandler
    .word   USART3_IRQHandler
    .word   EXTI15_10_IRQHandler
    .word   RTCAlarm_IRQHandler
    .word   USBWakeUp_IRQHandler
    .word   USBFS_IRQHandler
    .word   USBFSWakeUp_IRQHandler
    .word   UART4_IRQHandler
    .word   DMA1_Channel8_IRQHandler
#else
    .word   I2C1_EV_IRQHandler
    .word   I2C1_ER_IRQHandler
    .word   USART1_IRQHandler
    .word   SPI1_IRQHandler
    .word   TIM1_BRK_IRQHandler
    .word   TIM1_UP_IRQHandler
    .word   TIM1_TRG_COM_IRQHandler
    .word   TIM1_CC_IRQHandler
    .word   TIM2_IRQHandler
    .word   USART2_IRQHandler          /* USART2 (CH32V005/V006) */
    .word   OPCM_IRQHandler            /* OPCM (CH32V005/V006) */
#endif
    .option rvc

    .section .text.handle_reset, "ax", @progbits
    .weak handle_reset
    .align 1
handle_reset:
.option push
.option norelax
    la      gp, __global_pointer$
.option pop
    la      sp, _stack_end

    /* Load data section from flash to RAM */
    la      a0, _data_flash
    la      a1, _data_start
    la      a2, _data_end
    bgeu    a1, a2, 2f
1:
    lw      t0, (a0)
    sw      t0, (a1)
    addi    a0, a0, 4
    addi    a1, a1, 4
    bltu    a1, a2, 1b
2:
    /* Clear bss section */
    la      a0, _bss_start
    la      a1, _bss_end
    bgeu    a0, a1, 2f
1:
    sw      zero, (a0)
    addi    a0, a0, 4
    bltu    a0, a1, 1b
2:
    /* Enable global interrupt and configure privileged mode */
    li      t0, 0x1880
    csrw    mstatus, t0
    /* Enable interrupt nesting and hardware stack (QingKe specific) */
    li      t0, 0x3
    csrw    0x804, t0
    /* Configure the interrupt vector table */
    la      t0, _start
    ori     t0, t0, 3
    csrw    mtvec, t0

    jal     SystemInit
    la      t0, ch32v_main
    csrw    mepc, t0
    mret

    .section .text.vector_handler, "ax", @progbits
    .weak NMI_Handler
    .weak HardFault_Handler
    .weak SysTick_Handler
    .weak SW_Handler
    .weak WWDG_IRQHandler
    .weak PVD_IRQHandler
    .weak TAMPER_IRQHandler
    .weak RTC_IRQHandler
    .weak FLASH_IRQHandler
    .weak RCC_IRQHandler
    .weak EXTI0_IRQHandler
    .weak EXTI1_IRQHandler
    .weak EXTI2_IRQHandler
    .weak EXTI3_IRQHandler
    .weak EXTI4_IRQHandler
    .weak EXTI7_0_IRQHandler
    .weak EXTI9_5_IRQHandler
    .weak EXTI15_10_IRQHandler
    .weak AWU_IRQHandler
    .weak DMA1_Channel1_IRQHandler
    .weak DMA1_Channel2_IRQHandler
    .weak DMA1_Channel3_IRQHandler
    .weak DMA1_Channel4_IRQHandler
    .weak DMA1_Channel5_IRQHandler
    .weak DMA1_Channel6_IRQHandler
    .weak DMA1_Channel7_IRQHandler
    .weak ADC1_IRQHandler
    .weak USB_HP_CAN1_TX_IRQHandler
    .weak USB_LP_CAN1_RX0_IRQHandler
    .weak CAN1_RX1_IRQHandler
    .weak CAN1_SCE_IRQHandler
    .weak TIM1_BRK_IRQHandler
    .weak TIM1_UP_IRQHandler
    .weak TIM1_TRG_COM_IRQHandler
    .weak TIM1_CC_IRQHandler
    .weak TIM2_IRQHandler
    .weak TIM3_IRQHandler
    .weak TIM4_IRQHandler
    .weak I2C1_EV_IRQHandler
    .weak I2C1_ER_IRQHandler
    .weak I2C2_EV_IRQHandler
    .weak I2C2_ER_IRQHandler
    .weak SPI1_IRQHandler
    .weak SPI2_IRQHandler
    .weak USART1_IRQHandler
    .weak USART2_IRQHandler
    .weak USART3_IRQHandler
    .weak OPCM_IRQHandler
    .weak RTCAlarm_IRQHandler
    .weak USBWakeUp_IRQHandler
    .weak USBFS_IRQHandler
    .weak USBFSWakeUp_IRQHandler
    .weak UART4_IRQHandler
    .weak DMA1_Channel8_IRQHandler
NMI_Handler:
HardFault_Handler:
SysTick_Handler:
SW_Handler:
WWDG_IRQHandler:
PVD_IRQHandler:
TAMPER_IRQHandler:
RTC_IRQHandler:
FLASH_IRQHandler:
RCC_IRQHandler:
EXTI0_IRQHandler:
EXTI1_IRQHandler:
EXTI2_IRQHandler:
EXTI3_IRQHandler:
EXTI4_IRQHandler:
EXTI7_0_IRQHandler:
EXTI9_5_IRQHandler:
EXTI15_10_IRQHandler:
AWU_IRQHandler:
DMA1_Channel1_IRQHandler:
DMA1_Channel2_IRQHandler:
DMA1_Channel3_IRQHandler:
DMA1_Channel4_IRQHandler:
DMA1_Channel5_IRQHandler:
DMA1_Channel6_IRQHandler:
DMA1_Channel7_IRQHandler:
ADC1_IRQHandler:
USB_HP_CAN1_TX_IRQHandler:
USB_LP_CAN1_RX0_IRQHandler:
CAN1_RX1_IRQHandler:
CAN1_SCE_IRQHandler:
TIM1_BRK_IRQHandler:
TIM1_UP_IRQHandler:
TIM1_TRG_COM_IRQHandler:
TIM1_CC_IRQHandler:
TIM2_IRQHandler:
TIM3_IRQHandler:
TIM4_IRQHandler:
I2C1_EV_IRQHandler:
I2C1_ER_IRQHandler:
I2C2_EV_IRQHandler:
I2C2_ER_IRQHandler:
SPI1_IRQHandler:
SPI2_IRQHandler:
USART1_IRQHandler:
USART2_IRQHandler:
USART3_IRQHandler:
OPCM_IRQHandler:
RTCAlarm_IRQHandler:
USBWakeUp_IRQHandler:
USBFS_IRQHandler:
USBFSWakeUp_IRQHandler:
UART4_IRQHandler:
DMA1_Channel8_IRQHandler:
1:
    j       1b

