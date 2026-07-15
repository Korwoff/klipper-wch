// SPI functions on CH32V
//
// Copyright (C) 2016-2025  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU GPLv3 license.

#include "board/io.h" // readb, writeb
#include "command.h" // shutdown
#include "gpio.h" // spi_setup
#include "internal.h" // gpio_peripheral
#include "sched.h" // sched_shutdown

struct spi_info {
    SPI_TypeDef *spi;
    uint8_t miso_pin, mosi_pin, sck_pin;
};

#if CONFIG_MACH_CH32V00x
  DECL_ENUMERATION("spi_bus", "spi1_PC7_PC6_PC5", 0);
  DECL_CONSTANT_STR("BUS_PINS_spi1_PC7_PC6_PC5", "PC7,PC6,PC5");
#else
  DECL_ENUMERATION("spi_bus", "spi1_PA6_PA7_PA5", 0);
  DECL_CONSTANT_STR("BUS_PINS_spi1_PA6_PA7_PA5", "PA6,PA7,PA5");
  DECL_ENUMERATION("spi_bus", "spi2_PB14_PB15_PB13", 1);
  DECL_CONSTANT_STR("BUS_PINS_spi2_PB14_PB15_PB13", "PB14,PB15,PB13");
#endif

static const struct spi_info spi_bus[] = {
#if CONFIG_MACH_CH32V00x
    { SPI1, GPIO('C', 7), GPIO('C', 6), GPIO('C', 5) },
#else
    { SPI1, GPIO('A', 6), GPIO('A', 7), GPIO('A', 5) },
    { SPI2, GPIO('B', 14), GPIO('B', 15), GPIO('B', 13) },
#endif
};

struct spi_config
spi_setup(uint32_t bus, uint8_t mode, uint32_t rate)
{
    if (bus >= ARRAY_SIZE(spi_bus))
        shutdown("Invalid spi bus");

    SPI_TypeDef *spi = spi_bus[bus].spi;
    if (!is_enabled_pclock((uint32_t)spi)) {
        enable_pclock((uint32_t)spi);
        gpio_peripheral(spi_bus[bus].miso_pin, GPIO_FUNCTION(0), 1);
        gpio_peripheral(spi_bus[bus].mosi_pin, GPIO_FUNCTION(0), 0);
        gpio_peripheral(spi_bus[bus].sck_pin, GPIO_FUNCTION(0), 0);
    }

    uint32_t pclk = get_pclock_frequency((uint32_t)spi);
    uint32_t div = 0;
    while ((pclk >> (div + 1)) > rate && div < 7)
        div++;
    // Mode bits map directly to CPHA (bit 0) and CPOL (bit 1); BR starts at bit 3
    uint32_t cr1 = ((mode & 0x3)
                    | (div << 3)
                    | SPI_CTLR1_SPE | SPI_CTLR1_MSTR
                    | SPI_CTLR1_SSM | SPI_CTLR1_SSI);

    return (struct spi_config){ .spi = spi, .spi_cr1 = cr1 };
}

void
spi_prepare(struct spi_config config)
{
    SPI_TypeDef *spi = config.spi;
    uint32_t cr1 = spi->CTLR1;
    if (cr1 == config.spi_cr1)
        return;
    spi->CTLR1 = cr1 & ~SPI_CTLR1_SPE;
    spi->CTLR1; // flush
    spi->CTLR1 = config.spi_cr1;
}

void
spi_transfer(struct spi_config config, uint8_t receive_data,
             uint8_t len, uint8_t *data)
{
    SPI_TypeDef *spi = config.spi;
    uint8_t *end = data + len;
    while (data < end) {
        writeb((void *)&spi->DATAR, *data);
        while (!(spi->STATR & SPI_STATR_RXNE))
            ;
        uint8_t rdata = readb((void *)&spi->DATAR);
        if (receive_data)
            *data = rdata;
        data++;
    }
    while (spi->STATR & SPI_STATR_BSY)
        ;
}

