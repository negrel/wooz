#ifndef _BOX_H
#define _BOX_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Box define a rectangle.
 */
struct wooz_box {
  int32_t x, y;
  int32_t width, height;
};

struct wooz_boxf {
  double x, y;
  double width, height;
};

#endif
