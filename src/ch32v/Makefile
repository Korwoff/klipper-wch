# Additional CH32V build rules

# Setup the toolchain (isolated env via CROSS_PREFIX override supported)
CROSS_PREFIX ?= riscv-none-elf-
# Use the cross preprocessor for the linker script (host cpp may not be installed)
CPP = $(CROSS_PREFIX)cpp

MCU := $(shell echo $(CONFIG_MCU))

# CH32V00x uses RV32EC, CH32V203 uses RV32IMAC (zicsr/zifencei for CSR/FENCE.I)
MARCH-$(CONFIG_MACH_CH32V00x) := rv32ec_zicsr_zifencei
MABI-$(CONFIG_MACH_CH32V00x)  := ilp32e
MARCH-$(CONFIG_MACH_CH32V20x) := rv32imac_zicsr_zifencei
MABI-$(CONFIG_MACH_CH32V20x)  := ilp32

CFLAGS-$(CONFIG_MACH_CH32V00x) += -march=$(MARCH-y) -mabi=$(MABI-y) -Ilib/ch32v00x -Wno-old-style-definition -Os
CFLAGS-$(CONFIG_MACH_CH32V20x) += -march=$(MARCH-y) -mabi=$(MABI-y) -Ilib/ch32v20x -Wno-old-style-definition
CFLAGS += $(CFLAGS-y) -D$(shell echo $(CONFIG_MCU) | tr a-z A-Z) -Ilib/fast-hash

CFLAGS_klipper.elf += -nostdlib -lgcc -lc_nano
CFLAGS_klipper.elf += -T $(OUT)src/ch32v/ch32v_link.ld
$(OUT)klipper.elf: $(OUT)src/ch32v/ch32v_link.ld

dirs-y += src/ch32v src/generic lib/fast-hash

# Core platform files
src-y += ch32v/main.c ch32v/irq.c ch32v/timer.c ch32v/watchdog.c
src-y += ch32v/clockline.c ch32v/gpio.c ch32v/gpioperiph.c
src-$(CONFIG_MACH_CH32V00x) += ch32v/ch32v00x.c
src-$(CONFIG_MACH_CH32V20x) += ch32v/ch32v20x.c
src-$(CONFIG_SERIAL) += ch32v/serial.c

# Assembly startup file (top Makefile only converts .c sources).
OBJS_klipper.elf += $(OUT)src/ch32v/boot.o
$(OUT)src/ch32v/boot.o: src/ch32v/boot.S $(OUT)autoconf.h
	@echo "  Assembling $@"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
src-y += generic/crc16_ccitt.c
src-$(CONFIG_SERIAL) += generic/serial_irq.c

# Optional peripherals
src-$(CONFIG_WANT_ADC) += ch32v/adc.c
src-$(CONFIG_WANT_SPI) += ch32v/spi.c
src-$(CONFIG_WANT_I2C) += ch32v/i2c.c
src-$(CONFIG_WANT_HARD_PWM) += ch32v/hard_pwm.c

canbus-src-y := generic/canserial.c ../lib/fast-hash/fasthash.c
canbus-src-$(CONFIG_MACH_CH32V20x) += ch32v/can.c
src-$(CONFIG_CANSERIAL) += $(canbus-src-y) generic/canbus.c ch32v/chipid.c

usb-src-$(CONFIG_MACH_CH32V20x) := ch32v/usbfs.c
src-$(CONFIG_USBSERIAL) += $(usb-src-y) generic/usb_cdc.c ch32v/chipid.c
src-$(CONFIG_USBCANBUS) += $(usb-src-y) $(canbus-src-y)
src-$(CONFIG_USBCANBUS) += generic/usb_canbus.c ch32v/chipid.c

# Binary output file rules
target-y += $(OUT)klipper.bin

$(OUT)klipper.bin: $(OUT)klipper.elf
	@echo "  Creating bin file $@"
	$(Q)$(OBJCOPY) -O binary $< $@

