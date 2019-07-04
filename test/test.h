#include <stddef.h>

/**
 * Register a test @a name defined with function @a function to be executed.
 *
 * This function isn't intended to be called directly. Instead to define a test
 * use the TEST() macro.
 *
 * @param name     The test name (with the <tt>test_</tt> prefix)
 * @param function The test function itself
 * @param file     The name of the file that the test is defined in
 * @param line     The line number that the test is defined on
 */
void test_define(
  const char *name, void (*function)(void), const char *file, size_t line
);

#define TEST(__name) \
  /* Define the test function prototype so that its address can be retrieved in
   * the test registration function */ \
  void test_##__name(void); \
  \
  /* Define the test registration function with __attribute__((constructor)) so
   * that it will be called automatically */ \
  __attribute__((constructor)) \
  void define_test_##__name(void) { \
    test_define("test_" #__name, test_##__name, __FILE__, __LINE__); \
  } \
  \
  /* Define the actual test function */ \
  void test_##__name(void)
