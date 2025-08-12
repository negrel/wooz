#include <limits.h>
#include <math.h>

#include "output-layout.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void get_output_layout_extents(struct wooz_state *state, struct wooz_box *box) {
  int32_t x1 = INT_MAX, y1 = INT_MAX;
  int32_t x2 = INT_MIN, y2 = INT_MIN;

  struct wooz_output *output;
  wl_list_for_each(output, &state->outputs, link) {
    if (output->logical_geometry.x < x1) {
      x1 = output->logical_geometry.x;
    }
    if (output->logical_geometry.y < y1) {
      y1 = output->logical_geometry.y;
    }
    if (output->logical_geometry.x + output->logical_geometry.width > x2) {
      x2 = output->logical_geometry.x + output->logical_geometry.width;
    }
    if (output->logical_geometry.y + output->logical_geometry.height > y2) {
      y2 = output->logical_geometry.y + output->logical_geometry.height;
    }
  }

  box->x = x1;
  box->y = y1;
  box->width = x2 - x1;
  box->height = y2 - y1;
}

double get_output_rotation(enum wl_output_transform transform) {
  switch (transform & ~WL_OUTPUT_TRANSFORM_FLIPPED) {
  case WL_OUTPUT_TRANSFORM_90:
    return M_PI / 2;
  case WL_OUTPUT_TRANSFORM_180:
    return M_PI;
  case WL_OUTPUT_TRANSFORM_270:
    return 3 * M_PI / 2;
  }
  return 0;
}

int get_output_flipped(enum wl_output_transform transform) {
  return transform & WL_OUTPUT_TRANSFORM_FLIPPED ? -1 : 1;
}

void guess_output_logical_geometry(struct wooz_output *output) {
  output->logical_geometry.x = output->geometry.x;
  output->logical_geometry.y = output->geometry.y;
  if (output->transform & WL_OUTPUT_TRANSFORM_90) {
	  int32_t tmp = output->geometry.width;
	  output->geometry.width = output->geometry.height;
	  output->geometry.height = tmp;
  }
  output->logical_geometry.width = output->geometry.width / output->scale;
  output->logical_geometry.height = output->geometry.height / output->scale;
  output->logical_scale = output->scale;
}
