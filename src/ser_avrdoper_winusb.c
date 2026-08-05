/*
 * avrdude - A Downloader/Uploader for AVR device programmers
 *
 * Windows WinUSB bulk backend for avrdoper-usb.
 */

#include <ac_cfg.h>

#if defined(WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <setupapi.h>
#include <usbiodef.h>
#include <winusb.h>
#include <wchar.h>
#include <wctype.h>

#include "avrdude.h"
#include "libavrdude.h"
#include "usbdevs.h"

#define AVRDOPER_WINUSB_INTERFACE_NUMBER 3
#define AVRDOPER_WINUSB_TIMEOUT_MS 10000

static const wchar_t *const avrdoper_default_guid_text = L"{9B0D1CA8-2D68-4BA2-9E72-401055510001}";

struct avrdoper_winusb_ctx {
  HANDLE dev_handle;
  WINUSB_INTERFACE_HANDLE usb_handle;
  UCHAR rep;
  UCHAR wep;
  ULONG max_xfer;
  wchar_t path[512];
};

static int wchar_ascii_caseeq(wchar_t a, wchar_t b) {
  return towupper(a) == towupper(b);
}

static const wchar_t *wcsistr_ascii(const wchar_t *haystack, const wchar_t *needle) {
  size_t nlen;

  if(haystack == NULL || needle == NULL || *needle == L'\0')
    return haystack;

  nlen = wcslen(needle);
  for(; *haystack; haystack++) {
    size_t i;

    for(i = 0; i < nlen; i++) {
      if(haystack[i] == L'\0' || !wchar_ascii_caseeq(haystack[i], needle[i]))
        break;
    }
    if(i == nlen)
      return haystack;
  }

  return NULL;
}

static void avrdoper_winusb_log_last_error(const char *what) {
  DWORD err = GetLastError();
  wchar_t *sysmsg = NULL;

  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL, err, 0, (LPWSTR) &sysmsg, 0, NULL);

  if(sysmsg != NULL) {
    pmsg_ext_error("%s failed with error %lu: %ls\n", what, (unsigned long) err, sysmsg);
    LocalFree(sysmsg);
  } else {
    pmsg_ext_error("%s failed with error %lu\n", what, (unsigned long) err);
  }
}

static void avrdoper_winusb_log_path(const wchar_t *path) {
  if(path != NULL && *path != L'\0')
    pmsg_notice2("WinUSB path candidate: %ls\n", path);
}

static int avrdoper_winusb_path_matches(const wchar_t *path, unsigned short vid,
  unsigned short pid, int want_interface) {
  wchar_t vidpid[64];
  wchar_t mi[16];

  _snwprintf(vidpid, sizeof vidpid/sizeof *vidpid, L"VID_%04X&PID_%04X", vid, pid);
  vidpid[sizeof vidpid/sizeof *vidpid - 1] = 0;
  _snwprintf(mi, sizeof mi/sizeof *mi, L"MI_%02X", want_interface);
  mi[sizeof mi/sizeof *mi - 1] = 0;

  return wcsistr_ascii(path, vidpid) != NULL && wcsistr_ascii(path, mi) != NULL;
}

static int avrdoper_winusb_parse_guid(const wchar_t *default_guid_text, GUID *guid) {
  wchar_t envbuf[96];
  DWORD n;
  HRESULT hr;

  n = GetEnvironmentVariableW(L"AVRDOPER_WINUSB_GUID", envbuf, sizeof envbuf/sizeof *envbuf);
  if(n > 0 && n < sizeof envbuf/sizeof *envbuf) {
    hr = CLSIDFromString(envbuf, guid);
    if(SUCCEEDED(hr))
      return 0;
    pmsg_warning("AVRDOPER_WINUSB_GUID=%ls is not a valid GUID, using built-in default\n", envbuf);
  }

  hr = CLSIDFromString((LPOLESTR) default_guid_text, guid);
  if(FAILED(hr)) {
    pmsg_error("invalid built-in WinUSB GUID %ls\n", default_guid_text);
    return -1;
  }

  return 0;
}

static int avrdoper_winusb_query_pipes(struct avrdoper_winusb_ctx *ctx, union filedescriptor *fd) {
  USB_INTERFACE_DESCRIPTOR ifdesc;
  WINUSB_PIPE_INFORMATION pipe;
  int found_in = 0, found_out = 0;

  if(!WinUsb_QueryInterfaceSettings(ctx->usb_handle, 0, &ifdesc)) {
    avrdoper_winusb_log_last_error("WinUsb_QueryInterfaceSettings");
    return -1;
  }

  for(UCHAR index = 0; index < ifdesc.bNumEndpoints; index++) {
    if(!WinUsb_QueryPipe(ctx->usb_handle, 0, index, &pipe)) {
      avrdoper_winusb_log_last_error("WinUsb_QueryPipe");
      return -1;
    }

    if(pipe.PipeType != UsbdPipeTypeBulk)
      continue;

    if(pipe.PipeId == (UCHAR) fd->usb.rep) {
      ctx->rep = pipe.PipeId;
      if(pipe.MaximumPacketSize > 0)
        ctx->max_xfer = pipe.MaximumPacketSize;
      found_in = 1;
    }

    if(pipe.PipeId == (UCHAR) fd->usb.wep) {
      ctx->wep = pipe.PipeId;
      if(pipe.MaximumPacketSize > 0 && (!ctx->max_xfer || pipe.MaximumPacketSize < ctx->max_xfer))
        ctx->max_xfer = pipe.MaximumPacketSize;
      found_out = 1;
    }
  }

  if(!found_in || !found_out) {
    pmsg_error("WinUSB interface is missing expected bulk endpoints IN=0x%02x OUT=0x%02x\n",
      fd->usb.rep, fd->usb.wep);
    return -1;
  }

  if(ctx->max_xfer == 0)
    ctx->max_xfer = fd->usb.max_xfer? (ULONG) fd->usb.max_xfer: 64;
  if(ctx->max_xfer > USBDEV_MAX_XFER_3)
    ctx->max_xfer = USBDEV_MAX_XFER_3;

  return 0;
}

static int avrdoper_winusb_set_timeouts(struct avrdoper_winusb_ctx *ctx) {
  ULONG timeout = AVRDOPER_WINUSB_TIMEOUT_MS;

  if(!WinUsb_SetPipePolicy(ctx->usb_handle, ctx->rep, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout)) {
    avrdoper_winusb_log_last_error("WinUsb_SetPipePolicy(IN)");
    return -1;
  }
  if(!WinUsb_SetPipePolicy(ctx->usb_handle, ctx->wep, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout)) {
    avrdoper_winusb_log_last_error("WinUsb_SetPipePolicy(OUT)");
    return -1;
  }

  return 0;
}

static int avrdoper_winusb_set_pipe_timeout(struct avrdoper_winusb_ctx *ctx, UCHAR pipe_id, ULONG timeout) {
  if(!WinUsb_SetPipePolicy(ctx->usb_handle, pipe_id, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout)) {
    avrdoper_winusb_log_last_error(pipe_id == ctx->rep?
      "WinUsb_SetPipePolicy(IN)": "WinUsb_SetPipePolicy(OUT)");
    return -1;
  }

  return 0;
}

static void avrdoper_winusb_destroy_ctx(struct avrdoper_winusb_ctx *ctx) {
  if(ctx == NULL)
    return;

  if(ctx->usb_handle != NULL)
    WinUsb_Free(ctx->usb_handle);
  if(ctx->dev_handle != NULL && ctx->dev_handle != INVALID_HANDLE_VALUE)
    CloseHandle(ctx->dev_handle);

  mmt_free(ctx);
}

static int avrdoper_winusb_try_guid(const GUID *guid, unsigned short vid, unsigned short pid,
  union filedescriptor *fd, struct avrdoper_winusb_ctx **out_ctx) {
  HDEVINFO info;
  SP_DEVICE_INTERFACE_DATA ifdata;
  DWORD index = 0;

  *out_ctx = NULL;
  info = SetupDiGetClassDevsW(guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if(info == INVALID_HANDLE_VALUE)
    return -1;

  ifdata.cbSize = sizeof(ifdata);
  while(SetupDiEnumDeviceInterfaces(info, NULL, guid, index++, &ifdata)) {
    DWORD needed = 0;
    PSP_DEVICE_INTERFACE_DETAIL_DATA_W detail;
    struct avrdoper_winusb_ctx *ctx;

    SetupDiGetDeviceInterfaceDetailW(info, &ifdata, NULL, 0, &needed, NULL);
    if(needed < sizeof(*detail))
      continue;

    detail = (PSP_DEVICE_INTERFACE_DETAIL_DATA_W) mmt_malloc(needed);
    detail->cbSize = sizeof(*detail);
    if(!SetupDiGetDeviceInterfaceDetailW(info, &ifdata, detail, needed, &needed, NULL)) {
      mmt_free(detail);
      continue;
    }

    if(!avrdoper_winusb_path_matches(detail->DevicePath, vid, pid, AVRDOPER_WINUSB_INTERFACE_NUMBER)) {
      mmt_free(detail);
      continue;
    }

    ctx = mmt_malloc(sizeof(*ctx));
    memset(ctx, 0, sizeof(*ctx));
    wcsncpy(ctx->path, detail->DevicePath, sizeof ctx->path/sizeof *ctx->path - 1);
    ctx->path[sizeof ctx->path/sizeof *ctx->path - 1] = 0;
    mmt_free(detail);

    ctx->dev_handle = CreateFileW(ctx->path,
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    if(ctx->dev_handle == INVALID_HANDLE_VALUE) {
      avrdoper_winusb_log_last_error("CreateFileW");
      avrdoper_winusb_destroy_ctx(ctx);
      continue;
    }

    if(!WinUsb_Initialize(ctx->dev_handle, &ctx->usb_handle)) {
      avrdoper_winusb_log_path(ctx->path);
      avrdoper_winusb_log_last_error("WinUsb_Initialize");
      avrdoper_winusb_destroy_ctx(ctx);
      continue;
    }

    if(avrdoper_winusb_query_pipes(ctx, fd) < 0 || avrdoper_winusb_set_timeouts(ctx) < 0) {
      avrdoper_winusb_destroy_ctx(ctx);
      continue;
    }

    fd->usb.handle = ctx;
    fd->usb.rep = ctx->rep;
    fd->usb.wep = ctx->wep;
    fd->usb.max_xfer = (int) ctx->max_xfer;
    *out_ctx = ctx;
    SetupDiDestroyDeviceInfoList(info);
    return 0;
  }

  SetupDiDestroyDeviceInfoList(info);
  return -1;
}

static struct avrdoper_winusb_ctx *avrdoper_winusb_get_ctx(const union filedescriptor *fd) {
  return (struct avrdoper_winusb_ctx *) fd->usb.handle;
}

static int avrdoper_winusb_open(const char *port, union pinfo pinfo, union filedescriptor *fd) {
  GUID custom_guid;
  struct avrdoper_winusb_ctx *ctx = NULL;

  (void) port;

  if(avrdoper_winusb_parse_guid(avrdoper_default_guid_text, &custom_guid) == 0) {
    if(avrdoper_winusb_try_guid(&custom_guid, pinfo.usbinfo.vid, pinfo.usbinfo.pid, fd, &ctx) == 0) {
      pmsg_notice2("opened avrdoper-usb via WinUSB path %ls\n", ctx->path);
      return 0;
    }
  }

  if(avrdoper_winusb_try_guid(&GUID_DEVINTERFACE_USB_DEVICE, pinfo.usbinfo.vid, pinfo.usbinfo.pid, fd, &ctx) == 0) {
    pmsg_notice2("opened avrdoper-usb via generic USB interface path %ls\n", ctx->path);
    return 0;
  }

  pmsg_error("did not find any WinUSB device avrdoper-usb (%04x:%04x, interface MI_%02X)\n",
    pinfo.usbinfo.vid, pinfo.usbinfo.pid, AVRDOPER_WINUSB_INTERFACE_NUMBER);
  return -1;
}

static void avrdoper_winusb_close(union filedescriptor *fd) {
  struct avrdoper_winusb_ctx *ctx = avrdoper_winusb_get_ctx(fd);

  fd->usb.handle = NULL;
  avrdoper_winusb_destroy_ctx(ctx);
}

static int avrdoper_winusb_send(const union filedescriptor *fd, const unsigned char *buf, size_t buflen) {
  struct avrdoper_winusb_ctx *ctx = avrdoper_winusb_get_ctx(fd);

  if(ctx == NULL)
    return -1;

  if(avrdoper_winusb_set_pipe_timeout(ctx, ctx->wep, AVRDOPER_WINUSB_TIMEOUT_MS) < 0)
    return -1;

  while(buflen > 0) {
    ULONG chunk = (ULONG) ((buflen < ctx->max_xfer)? buflen: ctx->max_xfer);
    ULONG written = 0;

    if(!WinUsb_WritePipe(ctx->usb_handle, ctx->wep, (PUCHAR) buf, chunk, &written, NULL)) {
      avrdoper_winusb_log_last_error("WinUsb_WritePipe");
      return -1;
    }
    if(written != chunk) {
      pmsg_error("WinUSB short write: %lu of %lu bytes\n",
        (unsigned long) written, (unsigned long) chunk);
      return -1;
    }

    buf += written;
    buflen -= written;
  }

  return 0;
}

static int avrdoper_winusb_recv_frame(const union filedescriptor *fd, unsigned char *buf, size_t nbytes) {
  struct avrdoper_winusb_ctx *ctx = avrdoper_winusb_get_ctx(fd);
  int total = 0;
  ULONG timeout;

  if(ctx == NULL)
    return -1;

  timeout = (ULONG) (((long) nbytes*100 > serial_recv_timeout)?
    (long) nbytes*100: serial_recv_timeout);
  if(timeout < AVRDOPER_WINUSB_TIMEOUT_MS)
    timeout = AVRDOPER_WINUSB_TIMEOUT_MS;
  if(avrdoper_winusb_set_pipe_timeout(ctx, ctx->rep, timeout) < 0)
    return -1;

  do {
    ULONG got = 0;

    if(nbytes == 0)
      return -1;

    if(!WinUsb_ReadPipe(ctx->usb_handle, ctx->rep, buf, (ULONG) nbytes, &got, NULL)) {
      avrdoper_winusb_log_last_error("WinUsb_ReadPipe");
      return -1;
    }
    if(got == 0) {
      pmsg_warning("WinUSB returned a zero-length packet while receiving frame\n");
      break;
    }

    buf += got;
    nbytes -= got;
    total += (int) got;

    if(got < ctx->max_xfer)
      break;
  } while(nbytes > 0);

  return total;
}

static int avrdoper_winusb_drain(const union filedescriptor *fd, int display) {
  (void) fd;
  (void) display;
  return 0;
}

static int avrdoper_winusb_set_dtr_rts(const union filedescriptor *fd, int is_on) {
  (void) fd;
  (void) is_on;
  pmsg_error("avrdoper-usb does not support DTR/RTS setting\n");
  return -1;
}

struct serial_device avrdoper_winusb_serdev = {
  .open = avrdoper_winusb_open,
  .close = avrdoper_winusb_close,
  .rawclose = avrdoper_winusb_close,
  .send = avrdoper_winusb_send,
  .recv = avrdoper_winusb_recv_frame,
  .drain = avrdoper_winusb_drain,
  .set_dtr_rts = avrdoper_winusb_set_dtr_rts,
  .flags = SERDEV_FL_NONE,
};

#endif
