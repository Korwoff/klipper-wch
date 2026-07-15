/********************************** (C) COPYRIGHT *******************************
 * File Name          : system_ch32v00x.h
 * Description        : CH32V003 system header for Klipper port.
 ********************************************************************************/
#ifndef __SYSTEM_CH32V00x_H
#define __SYSTEM_CH32V00x_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t SystemCoreClock;

extern void SystemInit(void);
extern void SystemCoreClockUpdate(void);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_CH32V00x_H */
