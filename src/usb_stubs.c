/*
 * usb_stubs.c
 * -----------
 * Copyright : (c) 2009, Jeremie Dimino <jeremie@dimino.org>
 * Licence   : BSD3
 *
 * This file is a part of ocaml-usb.
 */

#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/custom.h>
#include <caml/alloc.h>
#include <caml/fail.h>
#include <caml/callback.h>
#include <libusb.h>
#include <poll.h>
#include <string.h>
#include <stdio.h>

/* +--------+
   | Errors |
   +--------+ */

void ml_usb_error(int code, char *fun_name)
{
#define fail(name) caml_raise(*caml_named_value("ocaml-usb:" name));
  switch(code) {
  case LIBUSB_ERROR_IO:
    fail("Failed");
  case LIBUSB_ERROR_INVALID_PARAM:
    caml_invalid_argument(fun_name);
  case LIBUSB_ERROR_ACCESS:
    fail("Access_denied");
  case LIBUSB_ERROR_NO_DEVICE:
    fail("No_device");
  case LIBUSB_ERROR_NOT_FOUND:
    caml_raise_not_found();
  case LIBUSB_ERROR_BUSY:
    fail("Device_busy");
  case LIBUSB_ERROR_TIMEOUT:
    fail("Timeout");
  case LIBUSB_ERROR_OVERFLOW:
    fail("Overflow");
  case LIBUSB_ERROR_PIPE:
    fail("Failed");
  case LIBUSB_ERROR_NO_MEM:
    caml_failwith("libusb: out of memory");
  case LIBUSB_ERROR_NOT_SUPPORTED:
    fail("Not_supported");
  default:
    caml_failwith("libusb");
  }
#undef fail
}

void *ml_usb_malloc(size_t size)
{
  void *ptr = malloc(size);
  if (ptr == NULL) caml_failwith("ocaml-usb: out of memory");
  return ptr;
}

struct libusb_transfer *ml_usb_alloc_transfer(int count)
{
  struct libusb_transfer *transfer = libusb_alloc_transfer(count);
  if (transfer == NULL) caml_failwith("ocaml-usb: out of memory");
  return transfer;
}

/* +----------------+
   | Initialization |
   +----------------+ */

void ml_usb_init()
{
  int res = libusb_init(NULL);
  if (res) ml_usb_error(res, "init");
}

void ml_usb_exit()
{
  libusb_exit(NULL);
}

void ml_usb_set_debug(value level)
{
  libusb_set_debug(NULL, Int_val(level));
}

/* +-------------------------+
   | Device and enumerations |
   +-------------------------+ */

#define Endpoint_val(endpoint, direction) (Int_val(endpoint) | (Int_val(direction) == 0 ? LIBUSB_ENDPOINT_IN : LIBUSB_ENDPOINT_OUT))

#define Ptr_val(v) ((long)(*(void**)Data_custom_val(v)))

int ml_usb_compare(value v1, value v2)
{
  return (int)(Ptr_val(v1) - Ptr_val(v2));
}

long ml_usb_hash(value v)
{
  return Ptr_val(v);
}

#define Device_val(v) *(libusb_device**)Data_custom_val(v)

void ml_usb_device_finalize(value dev)
{
  libusb_unref_device(Device_val(dev));
}

static struct custom_operations device_ops = {
  "usb.device",
  ml_usb_device_finalize,
  ml_usb_compare,
  ml_usb_hash,
  custom_serialize_default,
  custom_deserialize_default
};

#define Handle_val(v) *(libusb_device_handle**)Data_custom_val(v)

void ml_usb_device_handle_funalize(value handle)
{
  libusb_close(Handle_val(handle));
}

static struct custom_operations handle_ops = {
  "usb.device.handle",
  ml_usb_device_handle_funalize,
  ml_usb_compare,
  ml_usb_hash,
  custom_serialize_default,
  custom_deserialize_default
};

value alloc_device(libusb_device *device)
{
  value x = caml_alloc_custom(&device_ops, sizeof(libusb_device*), 0, 1);
  Device_val(x) = device;
  return x;
}

value alloc_handle(libusb_device_handle *handle)
{
  value x = caml_alloc_custom(&handle_ops, sizeof(libusb_device_handle*), 0, 1);
  Handle_val(x) = handle;
  return x;
}

CAMLprim value ml_usb_get_device_list(value unit)
{
  CAMLparam1(unit);
  libusb_device **devices;

  size_t cnt = libusb_get_device_list(NULL, &devices);
  if (cnt < 0)
    ml_usb_error(cnt, "get_device_list");

  /* Convert the array to a caml list */
  size_t i;
  value x = Val_int(0);
  for (i = 0; i < cnt; i++) {
    value y = caml_alloc_tuple(2);
    Store_field(y, 0, alloc_device(devices[i]));
    Store_field(y, 1, x);
    x = y;
  }

  /* Free the list but not the devices */
  libusb_free_device_list(devices, 0);

  CAMLreturn(x);
}

value ml_usb_get_bus_number(value dev)
{
  return Val_int(libusb_get_bus_number(Device_val(dev)));
}

value ml_usb_get_device_address(value dev)
{
  return Val_int(libusb_get_device_address(Device_val(dev)));
}

value ml_usb_get_max_packet_size(value dev, value direction, value endpoint)
{
  int res = libusb_get_max_packet_size(Device_val(dev), Endpoint_val(endpoint, direction));
  if (res) ml_usb_error(res, "get_max_packet_size");
  return Val_int(res);
}

CAMLprim value ml_usb_open(value dev)
{
  CAMLparam1(dev);
  libusb_device_handle *handle = NULL;
  int res = libusb_open(Device_val(dev), &handle);
  if (res) ml_usb_error(res, "open");
  CAMLreturn(alloc_handle(handle));
}

CAMLprim value ml_usb_open_device_with_vid_pid(value vid, value pid)
{
  CAMLparam2(vid, pid);
  libusb_device_handle *handle = libusb_open_device_with_vid_pid(NULL, Int_val(vid), Int_val(pid));
  if (handle == NULL)
    CAMLreturn(Val_int(0));
  else {
    value some = caml_alloc_tuple(1);
    Store_field(some, 0, alloc_handle(handle));
    CAMLreturn(some);
  }
}

value ml_usb_close(value handle)
{
  libusb_close(Handle_val(handle));
  return Val_unit;
}

CAMLprim value ml_usb_get_device(value handle)
{
  CAMLparam1(handle);
  libusb_device *device = libusb_get_device(Handle_val(handle));
  libusb_ref_device(device);
  CAMLreturn(alloc_device(device));
}

value ml_usb_claim_interface(value handle, value interface)
{
  int res = libusb_claim_interface(Handle_val(handle), Int_val(interface));
  if (res) ml_usb_error(res, "claim_interface");
  return Val_unit;
}

value ml_usb_release_interface(value handle, value interface)
{
  int res = libusb_release_interface(Handle_val(handle), Int_val(interface));
  if (res) ml_usb_error(res, "release_interface");
  return Val_unit;
}

value ml_usb_kernel_driver_active(value handle, value interface)
{
  int res = libusb_kernel_driver_active(Handle_val(handle), Int_val(interface));
  switch (res) {
  case 0:
    return Val_false;
  case 1:
    return Val_true;
  default:
    ml_usb_error(res, "kernel_driver_active");
    return Val_false;
  }
}

value ml_usb_detach_kernel_driver(value handle, value interface)
{
  int res = libusb_detach_kernel_driver(Handle_val(handle), Int_val(interface));
  if (res) ml_usb_error(res, "detach_kernel_driver");
  return Val_unit;
}

value ml_usb_attach_kernel_driver(value handle, value interface)
{
  int res = libusb_attach_kernel_driver(Handle_val(handle), Int_val(interface));
  if (res) ml_usb_error(res, "attach_kernel_driver");
  return Val_unit;
}

/* +------------------------+
   | Event-loop integration |
   +------------------------+ */

void ml_usb_handle_events()
{
  struct timeval tp = { 0, 0 };
  int res = libusb_handle_events_timeout(NULL, &tp);
  if (res) ml_usb_error(res, "handle_event_timeout");
}

/* Returns the list of file-descriptors that libusb want to monitors: */
CAMLprim value ml_usb_collect_sources(value lr /* List of file-descriptors to monitor for reading */,
                                      value lw /* List of file-descriptors to monitor for writing */)
{
  CAMLparam2(lr, lw);

  const struct libusb_pollfd **pollfds = libusb_get_pollfds(NULL);

  if (pollfds) {
    /* Add all fds to the two lists [lr] and [lw] */
    const struct libusb_pollfd **fds;
    for (fds = pollfds; *fds; fds++) {
      value fd = Val_int((*fds)->fd);
      short ev = (*fds)->events;
      if (ev & POLLIN) {
        value x = caml_alloc_tuple(2);
        Store_field(x, 0, fd);
        Store_field(x, 1, lr);
        lr = x;
      }
      if (ev & POLLOUT) {
        value x = caml_alloc_tuple(2);
        Store_field(x, 0, fd);
        Store_field(x, 1, lw);
        lw = x;
      }
      /* libusb only use [POLLIN] and [POLLOUT] */
    };
    free(pollfds);
  }

  struct timeval tp;
  int res = libusb_get_next_timeout(NULL, &tp);
  if (res == 1) {
    /* There is a timeout */
    value some = caml_alloc_tuple(1) /* [Some timeout] */;
    Store_field(some, 0, copy_double((double) tp.tv_sec + (double) tp.tv_usec / 1e6));
    value result = caml_alloc_tuple(3);
    Store_field(result, 0, lr);
    Store_field(result, 1, lw);
    Store_field(result, 2, some);
    CAMLreturn(result);
  }

  /* Something bad happen */
  if (res != 0) ml_usb_error(res, "get_next_timeout");

  /* There is no timeout */
  value result = caml_alloc_tuple(3);
  Store_field(result, 0, lr);
  Store_field(result, 1, lw);
  Store_field(result, 2, Val_int(0) /* [None] */);
  CAMLreturn(result);
}

/* +-----+
   | IOs |
   +-----+ */

/* Allocate a buffer, taking cares of remarks about overflows from the
   libsub documentation: */
unsigned char *ml_usb_alloc_buffer(int length)
{
  int rest = length % 512;
  if (rest) length = length - rest + 512;
  return (unsigned char*)ml_usb_malloc(length);
}

/* Convert an error transfer status to an exception */
value ml_usb_transfer_error(enum libusb_transfer_status status)
{
  char *name;
  switch(status) {
  case LIBUSB_TRANSFER_TIMED_OUT:
    name = "ocaml-usb:Timeout";
    break;
  case LIBUSB_TRANSFER_STALL:
    name = "ocaml-usb:Stalled";
    break;
  case LIBUSB_TRANSFER_NO_DEVICE:
    name = "ocaml-usb:No_device";
    break;
  case LIBUSB_TRANSFER_OVERFLOW:
    name = "ocaml-usb:Overflow";
    break;
  default:
    name = "ocaml-usb:Failed";
    break;
  }
  return *caml_named_value(name);
}

/* Handler for device-to-host transfers: */
void ml_usb_handle_recv(struct libusb_transfer *transfer)
{
  /* Retreive metadata: */
  value meta = (value)(transfer->user_data);

  /* Location where to output received bytes: */
  char *dest = String_val(Field(meta, 0)) + Long_val(Field(meta, 1));

  /* The caml callback function: */
  value caml_func = Field(meta, 2);

  /* Unregister the memory root: */
  caml_remove_generational_global_root((value*)(&(transfer->user_data)));

  value result;
  if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
    /* Copy bytes from the C memory to the caml string: */
    memcpy(dest, transfer->buffer, transfer->actual_length);
    /* Returns [OK actual_length] */
    result = caml_alloc(1, 0);
    Store_field(result, 0, Val_int(transfer->actual_length));
  } else {
    /* Returns [Error status] */
    result = caml_alloc(1, 1);
    Store_field(result, 0, ml_usb_transfer_error(transfer->status));
  }

  /* Cleanup allocated structures: */
  free(transfer->buffer);
  libusb_free_transfer(transfer);

  /* Call the ocaml handler: */
  caml_callback(caml_func, result);
}

/* Handler for host-to-device transfers: */
void ml_usb_handle_send(struct libusb_transfer *transfer)
{
  /* Metadata contains only the caml callback: */
  value caml_func = (value)(transfer->user_data);

  /* Unregister the memory root: */
  caml_remove_generational_global_root((value*)(&(transfer->user_data)));

  value result;
  if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
    result = caml_alloc(1, 0);
    Store_field(result, 0, Val_int(transfer->actual_length));
  } else {
    /* Returns [Error status] */
    result = caml_alloc(1, 1);
    Store_field(result, 0, ml_usb_transfer_error(transfer->status));
  }

  /* Cleanup allocated structures: */
  free(transfer->buffer);
  libusb_free_transfer(transfer);

  /* Call the ocaml handler: */
  caml_callback(caml_func, result);
}

/* Alloc a transfer and fill it with common informations: */
struct libusb_transfer *ml_usb_transfer(value desc /* the description provided by the caml function: */,
                                        value meta /* metadata for the callback */)
{
  struct libusb_transfer *transfer = ml_usb_alloc_transfer(0);
  transfer->dev_handle = Handle_val(Field(desc, 0));
  transfer->endpoint = Int_val(Field(desc, 1)) | LIBUSB_ENDPOINT_IN;
  transfer->timeout = Int_val(Field(desc, 2));
  transfer->buffer = ml_usb_alloc_buffer(Int_val(Field(desc, 5)));
  transfer->length = Int_val(Field(desc, 5));
  transfer->user_data = (void*)meta;

  /* Register metadata as a memory root, because we need it for the
     callback which will be called later: */
  caml_register_generational_global_root((value*)(&(transfer->user_data)));

  return transfer;
}

/* Device-to-host transfers, for interrupt or bulk transfers: */
CAMLprim value ml_usb_recv(value desc, enum libusb_transfer_type type)
{
  CAMLparam1(desc);

  /* Metadata for the transfer:  */
  value meta = caml_alloc_tuple(3);
  /* - the caml callback: */
  Store_field(meta, 0, Field(desc, 6));
  /* - the caml buffer: */
  Store_field(meta, 1, Field(desc, 3));
  /* - the offset in the buffer: */
  Store_field(meta, 2, Field(desc, 4));

  struct libusb_transfer *transfer = ml_usb_transfer(desc, meta);
  transfer->callback = ml_usb_handle_recv;
  transfer->type = type;

  int res = libusb_submit_transfer(transfer);
  if (res) ml_usb_error(res, "submit_transfer");
  CAMLreturn(Val_unit);
}

/* Host-to-device transfers, for interrupt or bulk transfers: */
CAMLprim value ml_usb_send(value desc, enum libusb_transfer_type type)
{
  CAMLparam1(desc);

  /* Metadata contains only the callback: */
  struct libusb_transfer *transfer = ml_usb_transfer(desc, Field(desc, 6));
  transfer->callback = ml_usb_handle_send;
  transfer->type = type;

  /* Copy data to send from the managed memory to the C memory: */
  memcpy(transfer->buffer, String_val(Field(desc, 3)) + Long_val(Field(desc, 4)), Long_val(Field(desc, 5)));

  int res = libusb_submit_transfer(transfer);
  if (res) ml_usb_error(res, "submit_transfer");
  CAMLreturn(Val_unit);
}

value ml_usb_bulk_recv(value desc)
{
  return ml_usb_recv(desc, LIBUSB_TRANSFER_TYPE_BULK);
}

value ml_usb_bulk_send(value desc)
{
  return ml_usb_send(desc, LIBUSB_TRANSFER_TYPE_BULK);
}

value ml_usb_interrupt_recv(value desc)
{
  return ml_usb_recv(desc, LIBUSB_TRANSFER_TYPE_INTERRUPT);
}

value ml_usb_interrupt_send(value desc)
{
  return ml_usb_send(desc, LIBUSB_TRANSFER_TYPE_INTERRUPT);
}
