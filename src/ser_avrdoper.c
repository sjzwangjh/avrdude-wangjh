/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 * Copyright (C) 2003-2004 Theodore A. Roth <troth@openavr.org>
 * Copyright (C) 2006 Joerg Wunsch <j@uriah.heep.sax.de>
 * Copyright (C) 2006 Christian Starkjohann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// Serial Interface emulation for USB programmer "AVR-Doper" in HID mode

#include <ac_cfg.h>

#if defined(HAVE_LIBHIDAPI)
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif
#include <hidapi/hidapi.h>

#include "avrdude.h"
#include "libavrdude.h"

// - -----------------------------------------------------------------------

// Numeric constants for 'reportType' parameters
#define USB_HID_REPORT_TYPE_INPUT   1
#define USB_HID_REPORT_TYPE_OUTPUT  2
#define USB_HID_REPORT_TYPE_FEATURE 3

/* These are the error codes which can be returned by functions of this
 * module.
 */
#define USB_ERROR_NONE      0
#define USB_ERROR_ACCESS    1
#define USB_ERROR_NOTFOUND  2
#define USB_ERROR_BUSY      16
#define USB_ERROR_IO        5

#define USB_VENDOR_ID   0x16c0
#define USB_PRODUCT_ID  0x05df
#define USB_INTERFACE_AVRDOPER_HID 0

static const char *usb_casestr(const char *haystack, const char *needle) {
  size_t needle_len;

  if(haystack == NULL || needle == NULL)
    return NULL;

  needle_len = strlen(needle);
  if(needle_len == 0)
    return haystack;

  for(; *haystack != 0; haystack++) {
    size_t i;

    for(i = 0; i < needle_len; i++) {
      unsigned char h = (unsigned char) haystack[i];
      unsigned char n = (unsigned char) needle[i];

      if(h == 0)
        return NULL;
      if(tolower(h) != tolower(n))
        break;
    }
    if(i == needle_len)
      return haystack;
  }

  return NULL;
}

static int usb_path_matches_interface(const char *path, int interface_number) {
  char needle[16];

  if(path == NULL)
    return 0;

  snprintf(needle, sizeof needle, "&mi_%02x", interface_number & 0xff);
  return usb_casestr(path, needle) != NULL;
}

/* HID EP1 max packet = 32 bytes. Use only reports 1 (15B) and 2 (31B). */
static const int reportDataSizes[2] = { 13, 29 };

/*
 * AVR-Doper's original HID transport defines up to 125 bytes of payload per
 * feature report. On this STM32 composite implementation, short commands are
 * stable but long programming frames can wedge the HID path. Keep each HID
 * feature transfer within a single 64-byte control packet (61 data bytes plus
 * report ID and length) to match the device-side buffering more conservatively.
 */
#define AVRDOPER_HID_SAFE_DATA_SIZE 29
#define AVRDOPER_HID_IO_RETRIES 5

static void avrdoper_sleep_ms(unsigned int ms) {
#if defined(_WIN32)
  Sleep(ms);
#else
  struct timespec req;

  req.tv_sec = ms / 1000;
  req.tv_nsec = (long) (ms % 1000) * 1000000L;
  nanosleep(&req, NULL);
#endif
}

// ------------------------------------------------------------------------

static int usbOpenDevice(union filedescriptor *fdp, int vendor, const char *vendorName,
  int product, const char *productName, int doReportIDs) {
  struct hid_device_info *list;
  struct hid_device_info *devinfo;
  hid_device *dev;

  (void) vendorName;
  (void) productName;
  (void) doReportIDs;

  list = hid_enumerate(vendor, product);
  if(list == NULL) {
    pmsg_ext_error("no HID device found for %04x:%04x\n", vendor, product);
    return USB_ERROR_NOTFOUND;
  }

  dev = NULL;
  for(devinfo = list; devinfo != NULL; devinfo = devinfo->next) {
    msg_trace("HID candidate path=%s interface=%d usage_page=0x%04x usage=0x%04x\n",
      devinfo->path != NULL? devinfo->path: "(null)",
      devinfo->interface_number,
      devinfo->usage_page,
      devinfo->usage);

    if(devinfo->interface_number >= 0 && devinfo->interface_number != USB_INTERFACE_AVRDOPER_HID)
      continue;

    if(devinfo->interface_number < 0 && !usb_path_matches_interface(devinfo->path, USB_INTERFACE_AVRDOPER_HID))
      continue;

    dev = hid_open_path(devinfo->path);
    if(dev == NULL) {
      pmsg_error("unable to open HID path %s: %ls\n",
        devinfo->path != NULL? devinfo->path: "(null)",
        hid_error(NULL));
      continue;
    }
    break;
  }

  hid_free_enumeration(list);
  if(dev == NULL) {
    pmsg_ext_error("no matching avrdoper HID interface found for %04x:%04x (MI_%02x)\n",
      vendor, product, USB_INTERFACE_AVRDOPER_HID);
    return USB_ERROR_NOTFOUND;
  }
  fdp->usb.handle = dev;
  return USB_ERROR_NONE;
}

// -------------------------------------------------------------------------

static void usbCloseDevice(union filedescriptor *fdp) {
  hid_device *udev = (hid_device *) fdp->usb.handle;

  fdp->usb.handle = NULL;

  if(udev == NULL)
    return;

  hid_close(udev);
}

// -------------------------------------------------------------------------

static int usbSetReport(const union filedescriptor *fdp, int reportType, char *buffer, int len) {
  hid_device *udev = (hid_device *) fdp->usb.handle;
  int bytesSent = -1;
  int attempt;

  for(attempt = 0; attempt < AVRDOPER_HID_IO_RETRIES; attempt++) {
    switch(reportType) {
    case USB_HID_REPORT_TYPE_INPUT:
      break;
    case USB_HID_REPORT_TYPE_OUTPUT:
      bytesSent = hid_write(udev, (unsigned char *) buffer, len);
      break;
    case USB_HID_REPORT_TYPE_FEATURE:
      bytesSent = hid_send_feature_report(udev, (unsigned char *) buffer, len);
      break;
    }

    if(bytesSent == len)
      return USB_ERROR_NONE;

    if(bytesSent < 0 && attempt + 1 < AVRDOPER_HID_IO_RETRIES) {
      msg_notice2("avrdoper HID send retry %d/%d after error: %ls\n",
        attempt + 1, AVRDOPER_HID_IO_RETRIES, hid_error(udev));
      avrdoper_sleep_ms(2);
      continue;
    }
    break;
  }

  if(bytesSent != len) {
    if(bytesSent < 0)
      pmsg_error("unable to send message: %ls\n", hid_error(udev));
    return USB_ERROR_IO;
  }
  return USB_ERROR_NONE;
}

// -------------------------------------------------------------------------

static int usbGetReport(const union filedescriptor *fdp, int reportType, int reportNumber, char *buffer, int *len) {
  hid_device *udev = (hid_device *) fdp->usb.handle;
  int bytesReceived = -1;
  int requestLen = *len;
  int attempt;

  for(attempt = 0; attempt < AVRDOPER_HID_IO_RETRIES; attempt++) {
    *len = requestLen;
    switch(reportType) {
    case USB_HID_REPORT_TYPE_INPUT:
      bytesReceived = hid_read_timeout(udev, (unsigned char *) buffer, *len, 500);
      break;
    case USB_HID_REPORT_TYPE_OUTPUT:
      break;
    case USB_HID_REPORT_TYPE_FEATURE:
      buffer[0] = reportNumber;
      bytesReceived = hid_get_feature_report(udev, (unsigned char *) buffer, *len);
      break;
    }

    if(bytesReceived >= 0) {
      *len = bytesReceived;
      return USB_ERROR_NONE;
    }

    if(attempt + 1 < AVRDOPER_HID_IO_RETRIES) {
      msg_notice2("avrdoper HID recv retry %d/%d after error: %ls\n",
        attempt + 1, AVRDOPER_HID_IO_RETRIES, hid_error(udev));
      avrdoper_sleep_ms(2);
      continue;
    }
    break;
  }

  if(bytesReceived < 0) {
    pmsg_error("unable to send message: %ls\n", hid_error(udev));
    return USB_ERROR_IO;
  }
  *len = bytesReceived;
  return USB_ERROR_NONE;
}

// ------------------------------------------------------------------------

static void dumpBlock(const char *prefix, const unsigned char *buf, int len) {
  int i;

  if(len <= 8) {                // More compact format for short blocks
    msg_info("%s: %d bytes: ", prefix, len);
    for(i = 0; i < len; i++) {
      msg_info("%02x ", buf[i]);
    }
    msg_info(" \"");
    for(i = 0; i < len; i++)
      msg_info("%c", buf[i] >= 0x20 && buf[i] < 0x7f? buf[i]: '.');
    msg_info("\"\n");
  } else {
    msg_info("%s: %d bytes:\n", prefix, len);
    while(len > 0) {
      for(i = 0; i < 16; i++) {
        if(i < len) {
          msg_info("%02x ", buf[i]);
        } else {
          msg_info("   ");
        }
        if(i == 7)
          msg_info(" ");
      }
      msg_info("  \"");
      for(i = 0; i < 16 && i < len; i++)
        msg_info("%c", buf[i] >= 0x20 && buf[i] < 0x7f? buf[i]: '.');
      msg_info("\"\n");
      buf += 16;
      len -= 16;
    }
  }
}

static const char *usbErrorText(int usbErrno) {
  switch(usbErrno) {
  case USB_ERROR_NONE:
    return "success";
  case USB_ERROR_ACCESS:
    return "access denied";
  case USB_ERROR_NOTFOUND:
    return "device not found";
  case USB_ERROR_BUSY:
    return "device is busy";
  case USB_ERROR_IO:
    return "I/O Error";
  default:{
      char *buffer = avr_cc_buffer(32);

      sprintf(buffer, "unknown error %d", usbErrno);
      return buffer;
    }
  }
}

// -------------------------------------------------------------------------

static int avrdoper_open(const char *port, union pinfo pinfo, union filedescriptor *fdp) {
  int rval;
  char *vname = "obdev.at";
  char *devname = "AVR-Doper";

  rval = usbOpenDevice(fdp, USB_VENDOR_ID, vname, USB_PRODUCT_ID, devname, 1);
  if(rval != 0) {
    pmsg_ext_error("USB %s\n", usbErrorText(rval));
    return -1;
  }
  return 0;
}

// -------------------------------------------------------------------------

static void avrdoper_close(union filedescriptor *fdp) {
  usbCloseDevice(fdp);
}

// -------------------------------------------------------------------------

static int chooseDataSize(int len) {
  size_t i;
  int capped_len = len > AVRDOPER_HID_SAFE_DATA_SIZE? AVRDOPER_HID_SAFE_DATA_SIZE: len;

  for(i = 0; i < sizeof(reportDataSizes)/sizeof(reportDataSizes[0]); i++) {
    if(reportDataSizes[i] >= capped_len)
      return i;
  }
  return i - 1;
}

static int avrdoper_send(const union filedescriptor *fdp, const unsigned char *buf, size_t buflen) {
  if(buflen > INT_MAX) {
    pmsg_error("%s() called with too large buflen = %lu\n", __func__, (unsigned long) buflen);
    return -1;
  }
  if(verblevel >= MSG_TRACE)
    dumpBlock("Send", buf, buflen);
  while(buflen > 0) {
    unsigned char buffer[256];
    int rval, lenIndex = chooseDataSize(buflen);
    int thisLen = (int) buflen > reportDataSizes[lenIndex]? reportDataSizes[lenIndex]: (int) buflen;

    buffer[0] = lenIndex + 1;   // Report ID
    buffer[1] = thisLen;
    memcpy(buffer + 2, buf, thisLen);
    msg_trace("Sending %d bytes data chunk\n", thisLen);
    rval = usbSetReport(fdp, USB_HID_REPORT_TYPE_OUTPUT, (char *) buffer, reportDataSizes[lenIndex] + 2);
    if(rval != 0) {
      pmsg_error("USB %s\n", usbErrorText(rval));
      return -1;
    }
    avrdoper_sleep_ms(1);
    buflen -= thisLen;
    buf += thisLen;
  }
  return 0;
}

// -------------------------------------------------------------------------

static int avrdoperFillBuffer(const union filedescriptor *fdp) {
  int bytesPending = reportDataSizes[1];        // Guess how much data is buffered in device

  cx->sad_avrdoperRxPosition = cx->sad_avrdoperRxLength = 0;
  while(bytesPending > 0) {
    int len, usbErr, lenIndex = chooseDataSize(bytesPending);
    unsigned char buffer[128];

    len = sizeof(cx->sad_avrdoperRxBuffer) - cx->sad_avrdoperRxLength;  // Bytes remaining
    if(reportDataSizes[lenIndex] + 2 > len)     // Requested data would not fit into buffer
      break;
    len = reportDataSizes[lenIndex] + 2;
    usbErr = usbGetReport(fdp, USB_HID_REPORT_TYPE_INPUT, lenIndex + 1, (char *) buffer, &len);
    if(usbErr != 0) {
      pmsg_error("USB %s\n", usbErrorText(usbErr));
      return -1;
    }
    msg_trace("Received %d bytes data chunk of total %d\n", len - 2, buffer[1]);
    len -= 2;                   // Compensate for report ID and length byte
    bytesPending = buffer[1] - len;     // Amount still buffered
    if(len > buffer[1])         // Cut away padding
      len = buffer[1];
    if(cx->sad_avrdoperRxLength + len > (int) sizeof(cx->sad_avrdoperRxBuffer)) {
      pmsg_error("buffer overflow\n");
      return -1;
    }
    memcpy(cx->sad_avrdoperRxBuffer + cx->sad_avrdoperRxLength, buffer + 2, len);
    cx->sad_avrdoperRxLength += len;
  }
  return 0;
}

static int avrdoper_recv(const union filedescriptor *fdp, unsigned char *buf, size_t buflen) {
  unsigned char *p = buf;
  int remaining = buflen;

  while(remaining > 0) {
    int len, available = cx->sad_avrdoperRxLength - cx->sad_avrdoperRxPosition;

    if(available <= 0) {        // Buffer is empty
      if(avrdoperFillBuffer(fdp) < 0)
        return -1;
      continue;
    }
    len = remaining < available? remaining: available;
    memcpy(p, cx->sad_avrdoperRxBuffer + cx->sad_avrdoperRxPosition, len);
    p += len;
    remaining -= len;
    cx->sad_avrdoperRxPosition += len;
  }
  if(verblevel >= MSG_TRACE)
    dumpBlock("Receive", buf, buflen);
  return 0;
}

// -------------------------------------------------------------------------

static int avrdoper_drain(const union filedescriptor *fdp, int display) {
  do {
    if(avrdoperFillBuffer(fdp) < 0)
      return -1;
  } while(cx->sad_avrdoperRxLength > 0);
  return 0;
}

// -------------------------------------------------------------------------

static int avrdoper_set_dtr_rts(const union filedescriptor *fdp, int is_on) {
  pmsg_error("AVR-Doper does not support DTR/RTS setting\n");
  return -1;
}

// -------------------------------------------------------------------------

struct serial_device avrdoper_serdev = {
  .open = avrdoper_open,
  .close = avrdoper_close,
  .rawclose = avrdoper_close,
  .send = avrdoper_send,
  .recv = avrdoper_recv,
  .drain = avrdoper_drain,
  .set_dtr_rts = avrdoper_set_dtr_rts,
  .flags = SERDEV_FL_NONE,
};
#endif                          // defined(HAVE_LIBHIDAPI)
