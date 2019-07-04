#include "common.h"

int mx_voidp_eq(const void *a, const void *b) {
  return *(void **) a == b;
}
