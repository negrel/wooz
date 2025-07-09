#ifndef _OUTPUT_LAYOUT_H
#define _OUTPUT_LAYOUT_H

#include <wayland-client.h>

#include "wooz.h"

void get_output_layout_extents(struct wooz_state *state, struct wooz_box *box);
void apply_output_transform(enum wl_output_transform transform, int32_t *width,
                            int32_t *height);
double get_output_rotation(enum wl_output_transform transform);
int get_output_flipped(enum wl_output_transform transform);
void guess_output_logical_geometry(struct wooz_output *output);

#endif
