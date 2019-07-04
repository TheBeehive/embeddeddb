#ifndef MX_COMMON_H
#define MX_COMMON_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief A @c size_t sentinel to indicate the absence of an index into an array
 *
 * Because size_t can store the maximum size of a theoretically possible object
 * of any type (including array) and SIZE_MAX is the maximum value of an object
 * of size_t type, the largest possible index into an array of any type is
 * SIZE_MAX - 1. MX_ABSENT is defined as a sentinel to indicate an invalid index
 * and is equal to SIZE_MAX.
 */
#define MX_ABSENT SIZE_MAX

/// Return the maximum of @a a and @a b (as defined by the @c < operator)
#define MX_MINIMUM(a, b) ({ \
  __typeof__(a) _a = a; \
  __typeof__(b) _b = b; \
  _a < _b ? _a : _b; })

/// Return the maximum of @a a and @a b (as defined by the @c > operator)
#define MX_MAXIMUM(a, b) ({ \
  __typeof__(a) _a = a; \
  __typeof__(b) _b = b; \
  _a > _b ? _a : _b; })

/// Return a number indicating the result of comparing @a a and @a b
#define MX_CMP(a, b) ({ \
  __typeof__(a) _a = a; \
  __typeof__(b) _b = b; \
  _a < _b ? -1 : (_a > _b ? 1 : 0); })

typedef int (*mx_eq_f)(const void *a, const void *b);

typedef int (*mx_cmp_f)(const void *a, const void *b);

/// Return whether the pointer at @a a is equal to @a b
int mx_voidp_eq(const void *a, const void *b);

#if SIZE_MAX == ULLONG_MAX
#define mx_addz_overflow(a, b, c) __builtin_uaddll_overflow( \
  (unsigned long long) a, (unsigned long long) b, (unsigned long long *) c \
)
#define mx_subz_overflow(a, b, c) __builtin_usubll_overflow( \
  (unsigned long long) a, (unsigned long long) b, (unsigned long long *) c \
)
#define mx_mulz_overflow(a, b, c) __builtin_umulll_overflow( \
  (unsigned long long) a, (unsigned long long) b, (unsigned long long *) c \
)
#elif SIZE_MAX == UINT_MAX
#define mx_addz_overflow __builtin_uadd_overflow( \
  (unsigned int) a, (unsigned int) b, (unsigned int *) c \
)
#define mx_subz_overflow __builtin_usub_overflow( \
  (unsigned int) a, (unsigned int) b, (unsigned int *) c \
)
#define mx_mulz_overflow __builtin_umul_overflow( \
  (unsigned int) a, (unsigned int) b, (unsigned int *) c \
)
#elif SIZE_MAX == ULONG_MAX
#define mx_addz_overflow __builtin_uaddl_overflow( \
  (unsigned long) a, (unsigned long) b, (unsigned long *) c \
)
#define mx_subz_overflow __builtin_usubl_overflow( \
  (unsigned long) a, (unsigned long) b, (unsigned long *) c \
)
#define mx_mulz_overflow __builtin_umull_overflow( \
  (unsigned long) a, (unsigned long) b, (unsigned long *) c \
)
#else
#error size_t is not one of unsigned long long, unsigned int, or unsigned long
#endif

#endif /* MX_COMMON_H */
