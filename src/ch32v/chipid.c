// Support for extracting the hardware chip id on CH32V
//
// Copyright (C) 2019  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "autoconf.h" // CONFIG_MACH_CH32V00x
#include "generic/canserial.h" // canserial_set_uuid
#include "generic/usb_cdc.h" // usb_fill_serial
#include "generic/usbstd.h" // usb_string_descriptor
#include "sched.h" // DECL_INIT

#if CONFIG_MACH_CH32V00x
// CH32V005: UID at 0x1FFFF7C4, 4 bytes (8 hex chars)
#define CHIP_UID_ADDR ((void *)0x1FFFF7C4)
#define CHIP_UID_LEN 4
#else
// CH32V203: UID at 0x1FFFF7E8, 6 bytes (12 hex chars)
#define CHIP_UID_ADDR ((void *)0x1FFFF7E8)
#define CHIP_UID_LEN 6
#endif

static struct {
    struct usb_string_descriptor desc;
    uint16_t data[CHIP_UID_LEN * 2];
} cdc_chipid;

struct usb_string_descriptor *
usbserial_get_serialid(void)
{
   return &cdc_chipid.desc;
}

void
chipid_init(void)
{
    if (CONFIG_USB_SERIAL_NUMBER_CHIPID)
        usb_fill_serial(&cdc_chipid.desc, ARRAY_SIZE(cdc_chipid.data)
                        , CHIP_UID_ADDR);
    if (CONFIG_CANBUS)
        canserial_set_uuid(CHIP_UID_ADDR, CHIP_UID_LEN);
}
DECL_INIT(chipid_init);

