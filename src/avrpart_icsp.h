/*
 * avrpart_icsp.h - AVR device identity lookup interface
 * Device ids are extracted from avrdude-avr-init.xml in document order so the
 * index matches the STM32 firmware g_avrDeviceTable order.
 */

#ifndef __AVRPART_ICSP_H__
#define __AVRPART_ICSP_H__

#include <stdint.h>

/* avr_devcode() - used by stk500v2_set_device_id()
 * Return the AVR index (avrdude-avr-init.xml order) for a conf part id such as
 * "m64a" or "64ea28".  Returns 0xFFFF if the id is unknown.
 */
uint16_t avr_devcode(const char* part_id);

#endif /* AVRPART_ICSP_H */
