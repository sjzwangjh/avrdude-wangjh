/*
 * avrpart_icsp.c - AVR device identity lookup implementation
 * Uses an embedded constant table generated from avrdude-avr-init.xml, so no
 * runtime XML file is needed.  The index assignment is identical to the STM32
 * firmware g_avrDeviceTable:
 *
 *   index 0 = xml id "1200"    (desc AT90S1200)
 *   index 1 = xml id "128da28" (desc AVR128DA28)
 *   ...
 *   max index = AVR_DEVICE_COUNT - 1
 *
 * Call flow:
 *   1. stk500v2_set_device_id() calls avr_devcode("m64a") to get the index
 *   2. no init/free required
 */

#include "avrpart_icsp.h"
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif

/* Embedded device id table - generated from avrdude-avr-init.xml */
#include "avr_devicenames.inc"

uint16_t avr_devcode(const char* part_id) {
    if (!part_id) return 0xFFFF;

    for (uint16_t i = 0; i < AVR_DEVICE_COUNT; i++) {
        if (strcasecmp(avr_device_ids[i], part_id) == 0)
            return i;
    }
    return 0xFFFF;
}
