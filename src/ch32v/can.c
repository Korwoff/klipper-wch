// Serial over CAN emulation for CH32V boards.
//
// Copyright (C) 2019 Eug Krashtan <eug.krashtan@gmail.com>
// Copyright (C) 2020 Pontus Borg <glpontus@gmail.com>
// Copyright (C) 2021-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <string.h> // memcpy
#include "autoconf.h" // CONFIG_MACH_CH32V20x
#include "board/irq.h" // irq_disable, irq_enable, NVIC_EnableIRQ
#include "command.h" // DECL_CONSTANT_STR
#include "generic/canbus.h" // canbus_notify_tx
#include "generic/canserial.h" // CANBUS_ID_ADMIN
#include "internal.h" // enable_pclock
#include "sched.h" // DECL_INIT

#if CONFIG_MACH_CH32V20x

#if CONFIG_CH32V_CANBUS_PA11_PA12
 DECL_CONSTANT_STR("RESERVE_PINS_CAN", "PA11,PA12");
 #define GPIO_Rx GPIO('A', 11)
 #define GPIO_Tx GPIO('A', 12)
 #define CAN_REMAP 0
#endif
#if CONFIG_CH32V_CANBUS_PB8_PB9
 DECL_CONSTANT_STR("RESERVE_PINS_CAN", "PB8,PB9");
 #define GPIO_Rx GPIO('B', 8)
 #define GPIO_Tx GPIO('B', 9)
 #define CAN_REMAP AFIO_PCFR1_CAN_REMAP_REMAP2
#endif

#define SOC_CAN CAN1
#define FILTER_CAN CAN1
#define CAN_RX0_IRQn  USB_LP_CAN1_RX0_IRQn
#define CAN_RX1_IRQn  CAN1_RX1_IRQn
#define CAN_TX_IRQn   USB_HP_CAN1_TX_IRQn
#define CAN_SCE_IRQn  CAN1_SCE_IRQn
#define CAN_FUNCTION  GPIO_FUNCTION(9)

// The CH32V20x SDK defines most CAN register bits; only the position/mask
// constants needed for field access and the mailbox ID/length fields are missing.
#define CAN_TXMIR_TXRQ       ((uint32_t)0x00000001)
#define CAN_TXMIR_RTR        ((uint32_t)0x00000002)
#define CAN_TXMIR_IDE        ((uint32_t)0x00000004)
#define CAN_TXMIR_EXID_Pos   (3U)
#define CAN_TXMIR_STID_Pos   (21U)
#define CAN_TXMIR_STID_Msk   ((uint32_t)0xFFE00000)

#define CAN_RXMIR_RTR        ((uint32_t)0x00000002)
#define CAN_RXMIR_IDE        ((uint32_t)0x00000004)
#define CAN_RXMIR_EXID_Pos   (3U)
#define CAN_RXMIR_STID_Pos   (21U)
#define CAN_RXMIR_STID_Msk   ((uint32_t)0xFFE00000)

#define CAN_TXMDTR_DLC       ((uint32_t)0x0000000F)
#define CAN_RXMDTR_DLC       ((uint32_t)0x0000000F)
#define CAN_RXMDTR_FMI_Pos   (8U)

#define CAN_BTIMR_BRP_Pos    (0U)
#define CAN_BTIMR_BRP_Msk    ((uint32_t)0x000003FF)
#define CAN_BTIMR_TS1_Pos    (16U)
#define CAN_BTIMR_TS1_Msk    ((uint32_t)0x000F0000)
#define CAN_BTIMR_TS2_Pos    (20U)
#define CAN_BTIMR_TS2_Msk    ((uint32_t)0x00700000)
#define CAN_BTIMR_SJW_Pos    (24U)
#define CAN_BTIMR_SJW_Msk    ((uint32_t)0x03000000)

#define CAN_ERRSR_LEC_Pos    (4U)
#define CAN_ERRSR_LEC_Msk    ((uint32_t)0x00000070)

static inline void
can_set_remap(uint32_t remap)
{
    if (!remap)
        return;
    enable_pclock((uint32_t)AFIO);
    AFIO->PCFR1 = (AFIO->PCFR1 & ~AFIO_PCFR1_CAN_REMAP) | remap;
}

// Transmit a packet
int
canhw_send(struct canbus_msg *msg)
{
    uint32_t tsr = SOC_CAN->TSTATR;
    if (!(tsr & (CAN_TSTATR_TME0|CAN_TSTATR_TME1|CAN_TSTATR_TME2))) {
        // No space in transmit fifo - enable tx irq
        irq_disable();
        SOC_CAN->INTENR |= CAN_INTENR_TMEIE;
        irq_enable();
        return -1;
    }
    int mbox = 2;
    if (tsr & CAN_TSTATR_TME0)
        mbox = 0;
    else if (tsr & CAN_TSTATR_TME1)
        mbox = 1;
    CAN_TxMailBox_TypeDef *mb = &SOC_CAN->sTxMailBox[mbox];

    /* Set up the DLC */
    mb->TXMDTR = (mb->TXMDTR & 0xFFFFFFF0) | (msg->dlc & 0x0F);

    /* Set up the data field */
    mb->TXMDLR = msg->data32[0];
    mb->TXMDHR = msg->data32[1];

    /* Request transmission */
    uint32_t tir;
    if (msg->id & CANMSG_ID_EFF)
        tir = ((msg->id & 0x1fffffff) << CAN_TXMIR_EXID_Pos) | CAN_TXMIR_IDE;
    else
        tir = (msg->id & 0x7ff) << CAN_TXMIR_STID_Pos;
    tir |= msg->id & CANMSG_ID_RTR ? CAN_TXMIR_RTR : 0;
    mb->TXMIR = tir | CAN_TXMIR_TXRQ;
    return CANMSG_DATA_LEN(msg);
}

// Setup the receive packet filter
void
canhw_set_filter(uint32_t id)
{
    CAN_TypeDef *fcan = FILTER_CAN;
    /* Select the start slave bank */
    uint32_t fmr = fcan->FCTLR;
    if (FILTER_CAN != SOC_CAN)
        // Using CAN2 with filter on CAN1 - assign CAN2 to first filter
        fmr &= ~((uint32_t)0x3F << 8);
    fcan->FCTLR = fmr | CAN_FCTLR_FINIT;
    /* Initialisation mode for the filter */
    fcan->FWR = 0;

    if (CONFIG_CANBUS_FILTER) {
        uint32_t mask = CAN_TXMIR_STID_Msk | CAN_TXMIR_IDE | CAN_TXMIR_RTR;
        fcan->sFilterRegister[0].FR1 = CANBUS_ID_ADMIN << CAN_RXMIR_STID_Pos;
        fcan->sFilterRegister[0].FR2 = mask;
        fcan->sFilterRegister[1].FR1 = (id + 1) << CAN_RXMIR_STID_Pos;
        fcan->sFilterRegister[1].FR2 = mask;
        fcan->sFilterRegister[2].FR1 = id << CAN_RXMIR_STID_Pos;
        fcan->sFilterRegister[2].FR2 = mask;
    } else {
        fcan->sFilterRegister[0].FR1 = 0;
        fcan->sFilterRegister[0].FR2 = 0;
        id = 0;
    }

    /* 32-bit scale for the filter */
    fcan->FSCFGR = (1<<0) | (1<<1) | (1<<2);

    /* Filter activation */
    fcan->FWR = (1<<0) | (id ? (1<<1) | (1<<2) : 0);
    /* Leave the initialisation mode for the filter */
    fcan->FCTLR = fmr & ~CAN_FCTLR_FINIT;
}

static struct {
    uint32_t rx_error, tx_error;
} CAN_Errors;

// Report interface status
void
canhw_get_status(struct canbus_status *status)
{
    irqstatus_t flag = irq_save();
    uint32_t esr = SOC_CAN->ERRSR;
    uint32_t rx_error = CAN_Errors.rx_error, tx_error = CAN_Errors.tx_error;
    irq_restore(flag);

    status->rx_error = rx_error;
    status->tx_error = tx_error;
    if (esr & CAN_ERRSR_BOFF)
        status->bus_state = CANBUS_STATE_OFF;
    else if (esr & CAN_ERRSR_EPVF)
        status->bus_state = CANBUS_STATE_PASSIVE;
    else if (esr & CAN_ERRSR_EWGF)
        status->bus_state = CANBUS_STATE_WARN;
    else
        status->bus_state = 0;
}

// This function handles CAN global interrupts
void __visible
CAN_IRQHandler(void)
{
    if (SOC_CAN->RFIFO0 & CAN_RFIFO0_FMP0) {
        // Read and ack data packet
        CAN_FIFOMailBox_TypeDef *mb = &SOC_CAN->sFIFOMailBox[0];
        uint32_t rir = mb->RXMIR;
        struct canbus_msg msg;
        if (rir & CAN_RXMIR_IDE)
            msg.id = ((rir >> CAN_RXMIR_EXID_Pos) & 0x1fffffff) | CANMSG_ID_EFF;
        else
            msg.id = (rir >> CAN_RXMIR_STID_Pos) & 0x7ff;
        msg.id |= rir & CAN_RXMIR_RTR ? CANMSG_ID_RTR : 0;
        msg.dlc = mb->RXMDTR & CAN_RXMDTR_DLC;
        msg.data32[0] = mb->RXMDLR;
        msg.data32[1] = mb->RXMDHR;
        SOC_CAN->RFIFO0 = CAN_RFIFO0_RFOM0;

        // Process packet
        canbus_process_data(&msg);
    }

    // Check for transmit ready
    uint32_t ier = SOC_CAN->INTENR;
    if (ier & CAN_INTENR_TMEIE
        && SOC_CAN->TSTATR & (CAN_TSTATR_RQCP0|CAN_TSTATR_RQCP1|CAN_TSTATR_RQCP2)) {
        // Tx
        SOC_CAN->INTENR = ier & ~CAN_INTENR_TMEIE;
        canbus_notify_tx();
    }

    // Check for error irq
    uint32_t msr = SOC_CAN->STATR;
    if (msr & CAN_STATR_ERRI) {
        uint32_t esr = SOC_CAN->ERRSR;
        uint32_t lec = (esr & CAN_ERRSR_LEC_Msk) >> CAN_ERRSR_LEC_Pos;
        if (lec && lec != 7) {
            SOC_CAN->ERRSR = 7 << CAN_ERRSR_LEC_Pos;
            if (lec >= 3 && lec <= 5)
                CAN_Errors.tx_error += 1;
            else
                CAN_Errors.rx_error += 1;
        }
        SOC_CAN->STATR = CAN_STATR_ERRI;
    }
}

// The CH32V vector table has separate entries for each CAN interrupt source.
// Route all of them to the common handler above.
void __visible USB_HP_CAN1_TX_IRQHandler(void) { CAN_IRQHandler(); }
void __visible USB_LP_CAN1_RX0_IRQHandler(void) { CAN_IRQHandler(); }
void __visible CAN1_RX1_IRQHandler(void) { CAN_IRQHandler(); }
void __visible CAN1_SCE_IRQHandler(void) { CAN_IRQHandler(); }

static inline const uint32_t
make_btr(uint32_t sjw,
         uint32_t time_seg1,
         uint32_t time_seg2,
         uint32_t brp)
{
    return (((uint32_t)(sjw-1)) << CAN_BTIMR_SJW_Pos
            | ((uint32_t)(time_seg1-1)) << CAN_BTIMR_TS1_Pos
            | ((uint32_t)(time_seg2-1)) << CAN_BTIMR_TS2_Pos
            | ((uint32_t)(brp - 1)) << CAN_BTIMR_BRP_Pos);
}

static inline const uint32_t
compute_btr(uint32_t pclock, uint32_t bitrate)
{
    uint32_t bit_clocks = pclock / bitrate;

    uint32_t sjw = 2;
    uint32_t qs;
    for (qs = 18; qs > 9; qs--) {
        uint32_t brp_rem = bit_clocks % qs;
        if (brp_rem == 0)
            break;
    }
    uint32_t brp       = bit_clocks / qs;
    uint32_t time_seg2 = qs / 8;
    uint32_t time_seg1 = qs - (1 + time_seg2);

    return make_btr(sjw, time_seg1, time_seg2, brp);
}

void
can_init(void)
{
    // Enable clock
    enable_pclock((uint32_t)SOC_CAN);

    can_set_remap(CAN_REMAP);
    gpio_peripheral(GPIO_Rx, CAN_FUNCTION, 1);
    gpio_peripheral(GPIO_Tx, CAN_FUNCTION, 0);

    uint32_t pclock = get_pclock_frequency((uint32_t)SOC_CAN);

    uint32_t btr = compute_btr(pclock, CONFIG_CANBUS_FREQUENCY);

    /* Request initialisation */
    SOC_CAN->CTLR = CAN_CTLR_INRQ;
    /* Wait the acknowledge */
    while (!(SOC_CAN->STATR & CAN_STATR_INAK))
        ;

    SOC_CAN->BTIMR = btr;

    // TXFP makes packets posted to the TX mboxes transmit in chronological order
    // ABOM makes the hardware automatically leave bus-off state
    SOC_CAN->CTLR = CAN_CTLR_TXFP | CAN_CTLR_ABOM;
    /* Wait the acknowledge */
    while (SOC_CAN->STATR & CAN_STATR_INAK)
        ;

    /* Configure the CAN Filter */
    canhw_set_filter(0);

    /* Configure Interrupts */
    PFIC->IPRIOR[CAN_RX0_IRQn] = 0x00;
    NVIC_EnableIRQ(CAN_RX0_IRQn);
    PFIC->IPRIOR[CAN_RX1_IRQn] = 0x00;
    NVIC_EnableIRQ(CAN_RX1_IRQn);
    PFIC->IPRIOR[CAN_TX_IRQn] = 0x00;
    NVIC_EnableIRQ(CAN_TX_IRQn);
    PFIC->IPRIOR[CAN_SCE_IRQn] = 0x00;
    NVIC_EnableIRQ(CAN_SCE_IRQn);
    SOC_CAN->INTENR = CAN_INTENR_FMPIE0 | CAN_INTENR_ERRIE | CAN_INTENR_LECIE;
}
DECL_INIT(can_init);

#endif // CONFIG_MACH_CH32V20x

