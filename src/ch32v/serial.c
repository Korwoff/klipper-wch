// CH32V serial (USART) support
//
// Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_SERIAL_BAUD
#include "board/irq.h" // irq_save
#include "board/serial_irq.h" // serial_rx_byte
#include "command.h" // DECL_CONSTANT_STR
#include "internal.h" // enable_pclock, GPIO
#include "sched.h" // DECL_INIT

#if CONFIG_CH32V_SERIAL_USART1
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PA10,PA9");
  #define GPIO_Rx GPIO('A', 10)
  #define GPIO_Tx GPIO('A', 9)
  #define USARTx USART1
  #define USARTx_IRQn USART1_IRQn
  #define SERIAL_IRQ_HANDLER USART1_IRQHandler
  #define SERIAL_REMAP 0
#elif CONFIG_CH32V_SERIAL_USART1_ALT_PB7_PB6
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PB7,PB6");
  #define GPIO_Rx GPIO('B', 7)
  #define GPIO_Tx GPIO('B', 6)
  #define USARTx USART1
  #define USARTx_IRQn USART1_IRQn
  #define SERIAL_IRQ_HANDLER USART1_IRQHandler
  #define SERIAL_REMAP AFIO_PCFR1_USART1_REMAP
#elif CONFIG_CH32V_SERIAL_USART2
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PA3,PA2");
  #define GPIO_Rx GPIO('A', 3)
  #define GPIO_Tx GPIO('A', 2)
  #define USARTx USART2
  #define USARTx_IRQn USART2_IRQn
  #define SERIAL_IRQ_HANDLER USART2_IRQHandler
  #define SERIAL_REMAP 0
#elif CONFIG_CH32V_SERIAL_USART3
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PB11,PB10");
  #define GPIO_Rx GPIO('B', 11)
  #define GPIO_Tx GPIO('B', 10)
  #define USARTx USART3
  #define USARTx_IRQn USART3_IRQn
  #define SERIAL_IRQ_HANDLER USART3_IRQHandler
  #define SERIAL_REMAP 0
#elif CONFIG_CH32V_SERIAL_UART4
  DECL_CONSTANT_STR("RESERVE_PINS_serial", "PC11,PC10");
  #define GPIO_Rx GPIO('C', 11)
  #define GPIO_Tx GPIO('C', 10)
  #define USARTx UART4
  #define USARTx_IRQn UART4_IRQn
  #define SERIAL_IRQ_HANDLER UART4_IRQHandler
  #define SERIAL_REMAP 0
#endif

// USART register bit definitions (SDK does not provide these)
#define USART_CTLR1_UE       ((uint16_t)0x2000)
#define USART_CTLR1_RE       ((uint16_t)0x0004)
#define USART_CTLR1_TE       ((uint16_t)0x0008)
#define USART_CTLR1_RXNEIE   ((uint16_t)0x0020)
#define USART_CTLR1_TXEIE    ((uint16_t)0x0080)
#define USART_STATR_RXNE     ((uint16_t)0x0020)
#define USART_STATR_TXE      ((uint16_t)0x0080)

#define CR1_FLAGS (USART_CTLR1_UE | USART_CTLR1_RE | USART_CTLR1_TE | USART_CTLR1_RXNEIE)

void __visible
SERIAL_IRQ_HANDLER(void)
{
    uint32_t sr = USARTx->STATR;
    if (sr & USART_STATR_RXNE)
        serial_rx_byte(USARTx->DATAR);
    if (sr & USART_STATR_TXE && USARTx->CTLR1 & USART_CTLR1_TXEIE) {
        uint8_t data;
        int ret = serial_get_tx_byte(&data);
        if (ret)
            USARTx->CTLR1 = CR1_FLAGS;
        else
            USARTx->DATAR = data;
    }
}

void
serial_enable_tx_irq(void)
{
    USARTx->CTLR1 = CR1_FLAGS | USART_CTLR1_TXEIE;
}

void
serial_init(void)
{
    enable_pclock((uint32_t)USARTx);
    enable_pclock((uint32_t)AFIO);
    if (SERIAL_REMAP)
        AFIO->PCFR1 |= SERIAL_REMAP;

    uint32_t pclk = get_pclock_frequency((uint32_t)USARTx);
    uint32_t div = DIV_ROUND_CLOSEST(pclk, CONFIG_SERIAL_BAUD);
    USARTx->BRR = div;
    USARTx->CTLR3 = 0;
    USARTx->CTLR1 = CR1_FLAGS;

    PFIC->IPRIOR[USARTx_IRQn] = 0x00;
    NVIC_EnableIRQ(USARTx_IRQn);

    gpio_peripheral(GPIO_Rx, GPIO_FUNCTION(0), 1);
    gpio_peripheral(GPIO_Tx, GPIO_FUNCTION(0), 0);
}
DECL_INIT(serial_init);

