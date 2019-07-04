#ifndef MX_VECTOR_H
#define MX_VECTOR_H

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>

#include "common.h"

typedef void * mx_vector_t;

/// Allocate and initialize a vector for elements of size @a element_size
mx_vector_t mx_vector_create(size_t element_size);

/**
 * @brief Allocate and initialize a vector for @a length elements of size
 *        @a element_size
 *
 * This behaves like an atomic sequence of mx_vector_create(@a element_size)
 * followed by mx_vector_ensure(@a length) on the created vector.
 *
 * @return the vector on success; otherwise NULL
 */
mx_vector_t mx_vector_create_with(size_t element_size, size_t length);

/**
 * @brief Allocate and initialize a vector by duplicating @a source
 *
 * Attempt to create a vector with the same element_size, volume, and length as
 * @source. If this fails and the length of @source is less than its volume
 * then attempt to create a vector with the same element_size and length as
 * @source.
 *
 * If either of these are successful then memcpy() each element in @a source
 * into the new vector.
 *
 * @return the vector on success; otherwise NULL
 */
mx_vector_t mx_vector_duplicate(mx_vector_t source);

/// Raze and deallocate the @a vector
void mx_vector_delete(mx_vector_t vector);

/// Return the element_size of the @a vector
size_t mx_vector_element_size(mx_vector_t vector);

/// Return the volume of the @a vector
size_t mx_vector_volume(mx_vector_t vector);

/// Return the length of the @a vector
size_t mx_vector_length(mx_vector_t vector);

/// Return a pointer to the element in the @a vector at index @a i
void *mx_vector_at(mx_vector_t vector, size_t i);

/**
 * @brief Return the index of the element at @a elmt in the @a vector
 *
 * This does not compare @a elmt against the elements in the @a vector. @a elmt
 * must already be a pointer to an element in the @a vector.
 *
 * This is _almost_ the inverse of mx_vector_at(). Because of integer division:
 *   mx_vector_at(@a vector, mx_vector_index(@a elmt)) != @a elmt
 * If @a elmt is a pointer to the middle of an element in the @a vector.
 */
size_t mx_vector_index(mx_vector_t vector, void *elmt);

/// Copy the element at index @a i in the @a vector into @a elmt
void mx_vector_get(mx_vector_t vector, size_t i, void *elmt);

/// Copy the data at @a elmt into the @a vector at index @a i
void mx_vector_set(mx_vector_t vector, size_t i, void *elmt);

/// Swap the data at index @a i with the data at index @a j in the @a vector
void mx_vector_swap(mx_vector_t vector, size_t i, size_t j);

/// Move the data at index @a source to index @a target in the @a vector
void mx_vector_move(mx_vector_t vector, size_t target, size_t source);

/**
 * @brief Resize the @a vector to its @a volume
 *
 * This will fail if realloc() returns NULL. The C standard does not guarantee
 * that realloc() to a smaller size will be successful. Therefore this can fail
 * even if the requested @a volume is less than the current volume of the
 * @a vector. If the realloc() fails then the @a vector will be unmodified.
 *
 * If @a volume is less than the length of the @a vector then the @a vector will
 * be truncated and have its length reduced to @a volume.
 *
 * @return the resized vector on success; otherwise NULL
 */
mx_vector_t mx_vector_resize(mx_vector_t vector, size_t volume)
  __attribute__((warn_unused_result));

/**
 * @brief Resize the @a vector to its length
 *
 * If mx_vector_resize() fails then the @a vector will be returned umodified.
 *
 * @return the shrunk vector on success; otherwise the unmodified vector
 */
mx_vector_t mx_vector_shrink(mx_vector_t vector)
  __attribute__((warn_unused_result));

/**
 * @brief Ensure that the volume of the @a vector is at least @a length
 *
 * If the volume of the @a vector is less than @a length then
 * mx_vector_resize() will be called.  Preallocation will first be attempted in
 * order to accomodate further increases in length according to the formula:
 *   volume = (length * 8 + 3) / 5
 * If this preallocation fails then a resize to @a length will be attempted. If
 * this also fails then the @a vector will be unmodified.
 *
 * After a successful mx_vector_ensure() subsequent mx_vector_insert()s (and
 * mx_vector_append()s) into the vector are guaranteed to be successful as long
 * as the resultant length does not exceed @a length.
 *
 * Note that the @a vector does not remember this @a length. As a result calling
 * any functions that can decrease the volume of the @a vector such as:
 *   mx_vector_resize()
 *   mx_vector_shrink()
 *   mx_vector_remove()
 * Will invalidate this guarantee.
 *
 * @return the resultant vector on success; otherwise NULL
 */
mx_vector_t mx_vector_ensure(mx_vector_t vector, size_t length)
  __attribute__((warn_unused_result));

/**
 * @brief Insert the element at @a elmt into the @a vector at index @a i
 *
 * If @a elmt is NULL then random data will inserted.
 *
 * This will call mx_vector_ensure(). If that fails then the @a vector will be
 * unmodified.
 *
 * @return the resultant vector on success; otherwise NULL
 */
mx_vector_t mx_vector_insert(mx_vector_t vector, size_t i, void *elmt)
  __attribute__((warn_unused_result));

/**
 * @brief Insert @a n elements at @a elmt into the @a vector at index @a i
 *
 * If @a elmt is NULL then random data will inserted.
 *
 * This will call mx_vector_ensure(). If that fails then the @a vector will be
 * unmodified.
 *
 * This is more efficient than calling mx_vector_insert() @a n times as the
 * elements after @a i will be shifted only once.
 *
 * @return the resultant vector on success; otherwise NULL
 */
mx_vector_t
mx_vector_inject(mx_vector_t vector, size_t i, void *elmt, size_t n)
  __attribute__((warn_unused_result));

/**
 * @brief Remove the element at index @a i from the @a vector
 *
 * No matter what the element is first removed from the @a vector. Then if the
 * length of the @a vector is reduced such that:
 *   length <= (volume - 1) / 2
 * Then a deallocation will be attempted to reduce the volume to:
 *   volume = (length * 6 + 4) / 5
 * If this deallocation fails then the vector will be returned as is.
 *
 * @return the resultant vector
 */
mx_vector_t mx_vector_remove(mx_vector_t vector, size_t i)
  __attribute__((warn_unused_result));

/**
 * @brief Remove @a n elements at index @a i from the @a vector
 *
 * No matter what the elements are first removed from the @a vector. Then if the
 * length of the @a vector is reduced such that:
 *   length <= (volume - 1) / 2
 * Then a deallocation will be attempted to reduce the volume to:
 *   volume = (length * 6 + 4) / 5
 * If this deallocation fails then the vector will be returned as is.
 *
 * @return the resultant vector
 */
mx_vector_t mx_vector_excise(mx_vector_t vector, size_t i, size_t n)
  __attribute__((warn_unused_result));

/**
 * @brief Reduce the length of the @a vector to @a length
 *
 * No matter what elements are first removed from the tail of the @a vector
 * until its length is @a length. Then if the length of the @a vector is reduced
 * such that:
 *   length <= (volume - 1) / 2
 * Then a deallocation will be attempted to reduce the volume to:
 *   volume = (length * 6 + 4) / 5
 * If this deallocation fails then the vector will be returned as is.
 *
 * @return the resultant vector
 */
mx_vector_t mx_vector_truncate(mx_vector_t vector, size_t length)
  __attribute__((warn_unused_result));

/**
 * @brief Insert the element at @a elmt as the last element in the @a vector
 *
 * If @a elmt is NULL then random data will inserted.
 *
 * This will call mx_vector_ensure(). If that fails then the @a vector will be
 * unmodified.
 *
 * @return the resultant vector on success; otherwise NULL
 */
mx_vector_t mx_vector_append(mx_vector_t vector, void *elmt)
  __attribute__((warn_unused_result));

/**
 * @brief Append @a n elements from @a elmt to the @a vector
 *
 * If @a elmt is NULL then random data will inserted.
 *
 * This will call mx_vector_ensure(). If that fails then the @a vector will be
 * unmodified.
 *
 * @return the resultant vector on success; otherwise NULL
 */
mx_vector_t mx_vector_extend(mx_vector_t vector, void *elmt, size_t n)
  __attribute__((warn_unused_result));

/// Return a pointer to the last element in the @a vector
void *mx_vector_tail(mx_vector_t vector);

#define mx_vector_push mx_vector_append

/// Copy the last element in the @a vector to @a elmt and remove it
mx_vector_t mx_vector_pull(mx_vector_t vector, void *elmt)
  __attribute__((warn_unused_result));

/// Copy the first element in the @a vector to @a elmt and remove it
mx_vector_t mx_vector_shift(mx_vector_t vector, void *elmt)
  __attribute__((warn_unused_result));

/// Test if the elements in @a a and @a b are equal according to @a eqf
bool mx_vector_eq(mx_vector_t a, mx_vector_t b, mx_eq_f eqf);

/// Test if the elements in @a a and @a b differ according to @a eqf
bool mx_vector_ne(mx_vector_t a, mx_vector_t b, mx_eq_f eqf);

/// Sort the @a vector according to @a cpmf
void mx_vector_sort(mx_vector_t vector, mx_cmp_f cmpf);

void *mx_vector_in(mx_vector_t vector, void *elmt, mx_eq_f eqf, void *ante);

/// Find the first element in the @a vector for which @a eqf returns true
size_t mx_vector_find(mx_vector_t vector, mx_eq_f eqf, void *data);

size_t
mx_vector_find_next(mx_vector_t vector, size_t i, mx_eq_f eqf, void *data);

size_t
mx_vector_find_last(mx_vector_t vector, size_t i, mx_eq_f eqf, void *data);

void *mx_vector_search(mx_vector_t vector, void *elmt, mx_cmp_f cmpf);

/**
 * @brief Print debugging information about the @a vector
 *
 * If @a elmt_debug is not NULL then it will be used to print debugging
 * information about each element in the @a vector.
 */
void mx_vector_debug(mx_vector_t vector, void (*elmt_debug)(void *));

#endif /* MX_VECTOR_H */
