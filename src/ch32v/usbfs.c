// Hardware interface to CH32V20x USBFS controller
//
// Copyright (C) 2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include <string.h> // memcpy
#include "autoconf.h" // CONFIG_USBSERIAL
#include "board/internal.h" // SysTick
#include "board/irq.h" // irq_disable
#include "board/misc.h" // timer_from_us
#include "board/usb_cdc.h" // usb_notify_ep0
#include "board/usb_cdc_ep.h" // USB_CDC_EP_BULK_IN
#include "command.h" // DECL_CONSTANT_STR
#include "internal.h" // enable_pclock, USBFSD
#include "sched.h" // DECL_INIT

#if CONFIG_MACH_CH32V20x


/****************************************************************
 * Endpoint buffers
 ****************************************************************/

static uint8_t __aligned(4) ep0_rx_buf[USB_CDC_EP0_SIZE];
static uint8_t __aligned(4) ep0_tx_buf[USB_CDC_EP0_SIZE];
static uint8_t __aligned(4) ep1_tx_buf[USB_CDC_EP_BULK_IN_SIZE];
static uint8_t __aligned(4) ep2_rx_buf[USB_CDC_EP_BULK_OUT_SIZE];
static uint8_t __aligned(4) ep3_tx_buf[USB_CDC_EP_ACM_SIZE];


/****************************************************************
 * Endpoint state
 ****************************************************************/

static uint8_t ep0_setup_ready;
static uint8_t ep0_rx_ready;
static uint8_t ep0_rx_len;
static uint8_t ep0_tx_ready;
static uint8_t ep1_tx_ready;
static uint8_t ep2_rx_ready;
static uint8_t ep2_rx_len;
static uint8_t ep3_tx_ready;
static uint8_t set_address;


/****************************************************************
 * Internal helpers
 ****************************************************************/

static void
ep0_arm_rx(void)
{
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_ACK;
}

static void
ep0_arm_tx_nak(void)
{
    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_TOG | USBFS_UEP_T_RES_NAK;
}

static void
ep2_arm_rx(void)
{
    USBFSD->UEP2_RX_CTRL = USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_ACK;
}

static void
usb_reset(void)
{
    // Reset endpoint state
    ep0_setup_ready = 0;
    ep0_rx_ready = 0;
    ep0_tx_ready = 1;
    ep1_tx_ready = 1;
    ep2_rx_ready = 0;
    ep3_tx_ready = 1;
    set_address = 0;

    // Configure endpoint directions
    USBFSD->UEP4_1_MOD = USBFS_UEP1_TX_EN;
    USBFSD->UEP2_3_MOD = USBFS_UEP2_RX_EN | USBFS_UEP3_TX_EN;
    USBFSD->UEP5_6_MOD = 0;
    USBFSD->UEP7_MOD = 0;

    // Configure DMA addresses
    USBFSD->UEP0_DMA = (uint32_t)ep0_rx_buf;
    USBFSD->UEP1_DMA = (uint32_t)ep1_tx_buf;
    USBFSD->UEP2_DMA = (uint32_t)ep2_rx_buf;
    USBFSD->UEP3_DMA = (uint32_t)ep3_tx_buf;

    // Clear lengths and arm endpoints
    USBFSD->UEP0_TX_LEN = 0;
    USBFSD->UEP1_TX_LEN = 0;
    USBFSD->UEP3_TX_LEN = 0;

    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
    ep0_arm_rx();
    USBFSD->UEP1_TX_CTRL = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;
    ep2_arm_rx();
    USBFSD->UEP3_TX_CTRL = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;

    // Clear device address and enable interrupts
    USBFSD->DEV_ADDR = 0;
    USBFSD->INT_EN = USBFS_UIE_SUSPEND | USBFS_UIE_BUS_RST | USBFS_UIE_TRANSFER;
}


/****************************************************************
 * USB interface
 ****************************************************************/

int_fast8_t
usb_read_bulk_out(void *data, uint_fast8_t max_len)
{
    if (!ep2_rx_ready)
        return -1;
    uint8_t len = ep2_rx_len;
    if (len > max_len)
        len = max_len;
    memcpy(data, ep2_rx_buf, len);
    ep2_rx_ready = 0;
    ep2_arm_rx();
    return len;
}

int_fast8_t
usb_send_bulk_in(void *data, uint_fast8_t len)
{
    if (!ep1_tx_ready)
        return -1;
    if (len > USB_CDC_EP_BULK_IN_SIZE)
        len = USB_CDC_EP_BULK_IN_SIZE;
    memcpy(ep1_tx_buf, data, len);
    USBFSD->UEP1_TX_LEN = len;
    USBFSD->UEP1_TX_CTRL = (USBFSD->UEP1_TX_CTRL & ~USBFS_UEP_T_RES_MASK)
                           | USBFS_UEP_T_RES_ACK;
    ep1_tx_ready = 0;
    return len;
}

int_fast8_t
usb_read_ep0(void *data, uint_fast8_t max_len)
{
    if (ep0_setup_ready) {
        // Setup packets are read via usb_read_ep0_setup
        return -1;
    }
    if (!ep0_rx_ready)
        return -1;
    uint8_t len = ep0_rx_len;
    if (len > max_len)
        len = max_len;
    memcpy(data, ep0_rx_buf, len);
    ep0_rx_ready = 0;
    // Status IN will follow; arm TX for it
    ep0_arm_tx_nak();
    return len;
}

int_fast8_t
usb_read_ep0_setup(void *data, uint_fast8_t max_len)
{
    if (!ep0_setup_ready)
        return -1;
    uint8_t len = USB_CDC_EP0_SIZE;
    if (len > max_len)
        len = max_len;
    memcpy(data, ep0_rx_buf, len);
    ep0_setup_ready = 0;
    // Arm for optional data-out stage
    ep0_arm_rx();
    return len;
}

int_fast8_t
usb_send_ep0(const void *data, uint_fast8_t len)
{
    if (!ep0_tx_ready)
        return -1;
    if (len > USB_CDC_EP0_SIZE)
        len = USB_CDC_EP0_SIZE;
    memcpy(ep0_tx_buf, data, len);
    USBFSD->UEP0_DMA = (uint32_t)ep0_tx_buf;
    USBFSD->UEP0_TX_LEN = len;
    USBFSD->UEP0_TX_CTRL = (USBFSD->UEP0_TX_CTRL & ~USBFS_UEP_T_RES_MASK)
                           | USBFS_UEP_T_RES_ACK;
    ep0_tx_ready = 0;
    return len;
}

void
usb_stall_ep0(void)
{
    USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_STALL;
    USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_STALL;
    ep0_tx_ready = 0;
}

void
usb_set_address(uint_fast8_t addr)
{
    set_address = USBFS_UDA_GP_BIT | (addr & USBFS_USB_ADDR_MASK);
    usb_send_ep0(NULL, 0);
}

void
usb_set_configure(void)
{
    ep1_tx_ready = 1;
    ep2_rx_ready = 0;
    ep3_tx_ready = 1;
    USBFSD->UEP1_TX_LEN = 0;
    USBFSD->UEP1_TX_CTRL = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;
    USBFSD->UEP3_TX_LEN = 0;
    USBFSD->UEP3_TX_CTRL = USBFS_UEP_T_AUTO_TOG | USBFS_UEP_T_RES_NAK;
    ep2_arm_rx();
}


/****************************************************************
 * Interrupt handler
 ****************************************************************/

void __visible
USBFS_IRQHandler(void)
{
    uint8_t intfg = USBFSD->INT_FG;
    uint8_t intst = USBFSD->INT_ST;

    if (intfg & USBFS_UIF_TRANSFER) {
        uint8_t token = intst & USBFS_UIS_TOKEN_MASK;
        uint8_t ep = intst & USBFS_UIS_ENDP_MASK;

        if (token == USBFS_UIS_TOKEN_SETUP && ep == 0) {
            // SETUP packet received
            ep0_setup_ready = 1;
            ep0_rx_ready = 0;
            // Prepare toggles for data stage
            ep0_arm_tx_nak();
            USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_TOG | USBFS_UEP_R_RES_NAK;
            usb_notify_ep0();
        } else if (token == USBFS_UIS_TOKEN_IN) {
            if (ep == 0) {
                // EP0 IN complete
                ep0_tx_ready = 1;
                USBFSD->UEP0_TX_CTRL = (USBFSD->UEP0_TX_CTRL ^ USBFS_UEP_T_TOG)
                                       | USBFS_UEP_T_RES_NAK;
                if (set_address) {
                    USBFSD->DEV_ADDR = set_address;
                    set_address = 0;
                }
                usb_notify_ep0();
            } else if (ep == USB_CDC_EP_BULK_IN) {
                ep1_tx_ready = 1;
                usb_notify_bulk_in();
            } else if (ep == USB_CDC_EP_ACM) {
                ep3_tx_ready = 1;
                usb_notify_bulk_in();
            }
        } else if (token == USBFS_UIS_TOKEN_OUT) {
            if (ep == 0) {
                // EP0 OUT data or status
                ep0_rx_len = USBFSD->RX_LEN;
                ep0_rx_ready = 1;
                // Toggle expected toggle for next OUT
                USBFSD->UEP0_RX_CTRL ^= USBFS_UEP_R_TOG;
                usb_notify_ep0();
            } else if (ep == USB_CDC_EP_BULK_OUT) {
                ep2_rx_len = USBFSD->RX_LEN;
                ep2_rx_ready = 1;
                // NAK further packets until buffer consumed
                USBFSD->UEP2_RX_CTRL = USBFS_UEP_R_AUTO_TOG | USBFS_UEP_R_RES_NAK;
                usb_notify_bulk_out();
            }
        }

        // Clear transfer interrupt flag
        USBFSD->INT_FG = USBFS_UIF_TRANSFER;
    }

    if (intfg & USBFS_UIF_BUS_RST) {
        USBFSD->INT_FG = USBFS_UIF_BUS_RST;
        usb_reset();
    }

    if (intfg & USBFS_UIF_SUSPEND) {
        USBFSD->INT_FG = USBFS_UIF_SUSPEND;
    }
}


/****************************************************************
 * Setup
 ****************************************************************/

DECL_CONSTANT_STR("RESERVE_PINS_USB", "PA11,PA12");

void
usb_init(void)
{
    // Enable USBFS peripheral clock
    enable_pclock(USBFS_BASE);

    // Reset USBFS controller
    USBFSD->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    udelay(10);
    USBFSD->BASE_CTRL = 0;

    // Configure and enable USB device
    USBFSD->BASE_CTRL = USBFS_UC_DEV_PU_EN | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    usb_reset();
    USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;

    // Enable USBFS interrupt
    NVIC_EnableIRQ(USBFS_IRQn);
}
DECL_INIT(usb_init);

#endif // CONFIG_MACH_CH32V20x

