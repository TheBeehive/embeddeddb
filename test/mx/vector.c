#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "vector.h"

typedef struct _header_t {
  size_t element_size;
  size_t volume;
  size_t length;
  char data[];
} header_t;

mx_vector_t header_to_vector(header_t *header) {
  return (mx_vector_t) header->data;
}

header_t *vector_to_header(mx_vector_t vector) {
  return (header_t *) (vector - offsetof(header_t, data));
}

mx_vector_t mx_vector_create(size_t element_size) {
  header_t *header;

  if ((header = malloc(sizeof(header_t))) == NULL)
    return NULL;

  header->element_size = element_size;
  header->volume = 0;
  header->length = 0;

  return header_to_vector(header);
}

mx_vector_t mx_vector_create_with(size_t element_size, size_t length) {
  // just volume = (length * 8 + 3) / 5 avoiding intermediate overflow
  size_t volume, size;
  header_t *header;

  // if the volume doesn't overflow then attempt to allocate it
  if ((volume = length / 5 * 8 + ((length % 5) * 8 + 3) / 5) > length &&
      // calculate size and test for overflow
      !mx_mulz_overflow(volume, element_size, &size) &&
      !mx_addz_overflow(size, sizeof(header_t), &size) &&
      (header = malloc(size)) != NULL) {
    header->element_size = element_size;
    header->volume = volume;
    header->length = length;

    return header_to_vector(header);
  }

  if (mx_mulz_overflow(length, element_size, &size))
    return NULL;
  if (mx_addz_overflow(size, sizeof(header_t), &size))
    return NULL;
  if ((header = malloc(size)) == NULL)
    return NULL;

  header->element_size = element_size;
  header->volume = length;
  header->length = length;

  return header_to_vector(header);
}

mx_vector_t mx_vector_duplicate(mx_vector_t source) {
  header_t *header;

  size_t element_size = mx_vector_element_size(source);
  size_t volume = mx_vector_volume(source);
  size_t length = mx_vector_length(source);

  if ((header = malloc(sizeof(header_t) + volume * element_size)) == NULL) {
    if (length == volume)
      return NULL;
    if ((header = malloc(sizeof(header_t) + length * element_size)) == NULL)
      return NULL;
    header->volume = length;
  } else
    header->volume = volume;

  header->element_size = element_size;
  header->length = length;

  memcpy(header->data, source, length * element_size);

  return header_to_vector(header);
}

void mx_vector_delete(mx_vector_t vector) {
  free(vector_to_header(vector));
}

size_t mx_vector_element_size(mx_vector_t vector) {
  return vector_to_header(vector)->element_size;
}

size_t mx_vector_volume(mx_vector_t vector) {
  return vector_to_header(vector)->volume;
}

size_t mx_vector_length(mx_vector_t vector) {
  return vector_to_header(vector)->length;
}

void *mx_vector_at(mx_vector_t vector, size_t i) {
  return (char *) vector + i * mx_vector_element_size(vector);
}

size_t mx_vector_index(mx_vector_t vector, void *elmt) {
  return ((char *) elmt - (char *) vector) / mx_vector_element_size(vector);
}

void mx_vector_get(mx_vector_t vector, size_t i, void *elmt) {
  memcpy(elmt, mx_vector_at(vector, i), mx_vector_element_size(vector));
}

void mx_vector_set(mx_vector_t vector, size_t i, void *elmt) {
  memcpy(mx_vector_at(vector, i), elmt, mx_vector_element_size(vector));
}

void mx_vector_swap(mx_vector_t vector, size_t i, size_t j) {
  char *a = mx_vector_at(vector, i);
  char *b = mx_vector_at(vector, j);
  size_t element_size = mx_vector_element_size(vector);

  for (size_t k = 0; k < element_size; k++) {
    char buffer;
    buffer = a[k];
    a[k] = b[k];
    b[k] = buffer;
  }
}

void mx_vector_move(mx_vector_t vector, size_t target, size_t source) {
  if (target == source)
    return;

  if (target < source) {
    while (source-- > target)
      mx_vector_swap(vector, source, source + 1);
  } else {
    for (; source < target; source++)
      mx_vector_swap(vector, source, source + 1);
  }
}

mx_vector_t mx_vector_resize(mx_vector_t vector, size_t volume) {
  header_t *header = vector_to_header(vector);
  size_t size;

  // calculate size and test for overflow
  if (mx_mulz_overflow(volume, header->element_size, &size))
    return NULL;
  if (mx_addz_overflow(size, sizeof(header_t), &size))
    return NULL;

  if ((header = realloc(header, size)) == NULL)
    return NULL;

  header->volume = volume;
  header->length = MX_MINIMUM(header->length, volume);

  return header_to_vector(header);
}

mx_vector_t mx_vector_shrink(mx_vector_t vector) {
  mx_vector_t shrunk;

  shrunk = mx_vector_resize(vector, mx_vector_length(vector));

  return shrunk != NULL ? shrunk : vector;
}

mx_vector_t mx_vector_ensure(mx_vector_t vector, size_t length) {
  if (length > mx_vector_volume(vector)) {
    // just volume = (length * 8 + 3) / 5 avoiding intermediate overflow
    size_t volume = length / 5 * 8 + ((length % 5) * 8 + 3) / 5;

    // if the volume doesn't overflow then attempt to allocate it
    if (volume > length) {
      mx_vector_t resize;
      if ((resize = mx_vector_resize(vector, volume)) != NULL)
        return resize;
    }

    // if either the volume overflows or the allocation failed then attempt to
    // resize to just the length
    return mx_vector_resize(vector, length);
  } else return vector;
}

mx_vector_t mx_vector_insert(mx_vector_t vector, size_t i, void *elmt) {
  return mx_vector_inject(vector, i, elmt, 1);
}

mx_vector_t
mx_vector_inject(mx_vector_t vector, size_t i, void *elmt, size_t n) {
  size_t length = mx_vector_length(vector);

  if (mx_addz_overflow(length, n, &length))
    return NULL;

  if ((vector = mx_vector_ensure(vector, length)) == NULL)
    return NULL;

  // move the existing elements n elements toward the tail
  void *target = mx_vector_at(vector, i + n);
  void *source = mx_vector_at(vector, i + 0);
  size_t size = (mx_vector_length(vector) - i) * mx_vector_element_size(vector);
  memmove(target, source, size);

  if (elmt != NULL)
    memcpy(mx_vector_at(vector, i), elmt, n * mx_vector_element_size(vector));

  // increase the length
  vector_to_header(vector)->length = length;

  return vector;
}

mx_vector_t mx_vector_remove(mx_vector_t vector, size_t i) {
  return mx_vector_excise(vector, i, 1);
}

mx_vector_t mx_vector_excise(mx_vector_t vector, size_t i, size_t n) {
  size_t length = mx_vector_length(vector) - n;

  // move the existing elements n elements toward the head
  void *target = mx_vector_at(vector, i + 0);
  void *source = mx_vector_at(vector, i + n);
  size_t size = (length - i) * mx_vector_element_size(vector);
  memmove(target, source, size);

  if (length <= (mx_vector_volume(vector) - 1) / 2) {
    mx_vector_t resize;
    // just volume = (length * 6 + 4) / 5 avoiding intermediate overflow
    size_t volume = length / 5 * 6 + ((length % 5) * 6 + 4) / 5;

    if ((resize = mx_vector_resize(vector, volume)) != NULL)
      vector = resize;
  }

  // decrease the length
  vector_to_header(vector)->length = length;

  return vector;
}

mx_vector_t mx_vector_truncate(mx_vector_t vector, size_t length) {
  size_t n = mx_vector_length(vector) - length;
  return mx_vector_excise(vector, mx_vector_length(vector) - n, n);
}

mx_vector_t mx_vector_append(mx_vector_t vector, void *elmt) {
  return mx_vector_insert(vector, mx_vector_length(vector), elmt);
}

mx_vector_t mx_vector_extend(mx_vector_t vector, void *elmt, size_t n) {
  return mx_vector_inject(vector, mx_vector_length(vector), elmt, n);
}

void *mx_vector_tail(mx_vector_t vector) {
  return mx_vector_at(vector, mx_vector_length(vector) - 1);
}

mx_vector_t mx_vector_pull(mx_vector_t vector, void *elmt) {
  if (elmt != NULL)
    mx_vector_get(vector, mx_vector_length(vector) - 1, elmt);
  return mx_vector_remove(vector, mx_vector_length(vector) - 1);
}

mx_vector_t mx_vector_shift(mx_vector_t vector, void *elmt) {
  if (elmt != NULL)
    mx_vector_get(vector, 0, elmt);
  return mx_vector_remove(vector, 0);
}

bool mx_vector_eq(mx_vector_t a, mx_vector_t b, mx_eq_f eqf) {
  if (mx_vector_length(a) != mx_vector_length(b))
    return false;

  size_t length = mx_vector_length(a);

  // compare using memcpy if no equality function is available
  if (eqf == NULL) {
    if (mx_vector_element_size(a) != mx_vector_element_size(b))
      return false;
    size_t element_size = mx_vector_element_size(a);
    for (size_t i = 0; i < length; i++) {
      if (memcmp(mx_vector_at(a, i), mx_vector_at(b, i), element_size) != 0)
        return false;
    }
  } else {
    for (size_t i = 0; i < length; i++) {
      if (eqf(mx_vector_at(a, i), mx_vector_at(b, i)) == false)
        return false;
    }
  }

  return true;
}

bool mx_vector_ne(mx_vector_t a, mx_vector_t b, mx_eq_f eqf) {
  return !mx_vector_eq(a, b, eqf);
}

void mx_vector_sort(mx_vector_t vector, mx_cmp_f cmpf) {
  qsort(vector, mx_vector_length(vector), mx_vector_element_size(vector), cmpf);
}

void *mx_vector_in(mx_vector_t vector, void *elmt, mx_eq_f eqf, void *ante) {
  void *tail = mx_vector_tail(vector);
  ante = ante == NULL ? vector : (char *) ante + mx_vector_element_size(vector);

  while (ante <= tail) {
    if (eqf(ante, elmt))
      return ante;
    ante = (char *) ante + mx_vector_element_size(vector);
  }

  return NULL;
}

size_t mx_vector_find(mx_vector_t vector, mx_eq_f eqf, void *data) {
  return mx_vector_find_next(vector, 0, eqf, data);
}

size_t
mx_vector_find_next(mx_vector_t vector, size_t i, mx_eq_f eqf, void *data) {
  for (; i < mx_vector_length(vector); i++) {
    if (eqf(mx_vector_at(vector, i), data))
      return i;
  }
  return MX_ABSENT;
}

size_t
mx_vector_find_last(mx_vector_t vector, size_t i, mx_eq_f eqf, void *data) {
  for (; i-- > 0;) {
    if (eqf(mx_vector_at(vector, i), data))
      return i;
  }
  return MX_ABSENT;
}

void *mx_vector_search(mx_vector_t vector, void *elmt, mx_cmp_f cmpf) {
  size_t element_size = mx_vector_element_size(vector);
  return bsearch(elmt, vector, mx_vector_length(vector), element_size, cmpf);
}

void mx_vector_debug(mx_vector_t vector, void (*elmt_debug)(void *)) {
  header_t *header = vector_to_header(vector);
  fprintf(stderr,
    "mx_vector_t(data = %p, element_size = %zu, utilization = %zu/%zu)",
  header->data, header->element_size, header->length, header->volume);

  if (elmt_debug != NULL) {
    fprintf(stderr, " [ ");
    for (size_t i = 0; i < header->length; i++) {
      if (i > 0)
        fprintf(stderr, ", ");
      elmt_debug(mx_vector_at(vector, i));
    }
    fprintf(stderr, " ]");
  }
}
