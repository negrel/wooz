#ifndef _BUFFER_H
#define _BUFFER_H

#include <wayland-client.h>

struct wooz_buffer {
  struct wl_buffer *wl_buffer;
  void *data;
  int32_t width, height, stride;
  size_t size;
  enum wl_shm_format format;
};

struct wooz_buffer *create_buffer(struct wl_shm *shm, enum wl_shm_format format,
                                  int32_t width, int32_t height,
                                  int32_t stride);
void destroy_buffer(struct wooz_buffer *buffer);

#endif
