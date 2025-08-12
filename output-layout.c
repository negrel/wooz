#include <limits.h>

#include "output-layout.h"

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
