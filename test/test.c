#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "mx/vector.h"

#include "test.h"

typedef struct _test_t {
  const char *name;       ///< The test name (with the <tt>test_</tt> prefix)
  void (*function)(void); ///< The test function itself
  const char *file;       ///< The name of the file that the test is defined in
  size_t line;            ///< The line number that the test is defined on
} test_t;

/// Execute the @a test and return its result
bool test_execute(test_t *test);

/// The global vector of defined tests
test_t *v_test = NULL;

void test_define(
  const char *name, void (*function)(void), const char *file, size_t line
) {
  if (v_test == NULL)
    if ((v_test = mx_vector_create(sizeof(test_t))) == NULL) {
      fprintf(stderr, "unable to create test vector\n");
      exit(EXIT_FAILURE);
    }

  test_t test = {
    .name = name, .function = function, .file = file, .line = line,
  };

  if ((v_test = mx_vector_append(v_test, &test)) == NULL) {
    mx_vector_delete(v_test);
    fprintf(stderr, "unable to define test %s\n", name);
    exit(EXIT_FAILURE);
  }
}

bool test_execute(test_t *test) {
  pid_t runner;

  // Fork a test runner
  if ((runner = fork()) < 0) {
    fprintf(stderr, "unable to fork test runner of test %s\n", test->name);
    exit(EXIT_FAILURE);
  }

  // If we're in the child process then call the test function and exit with a
  // successful exit code. A test failure should exit with an unsuccessful exit
  // code in the test function itself.
  if (runner == 0) {
    test->function();
    exit(EXIT_SUCCESS);
  }

  int status;
  pid_t e;

  // Wait for the runner to terminate either through normal termination or the
  // receipt of a signal. If the wait call is interrupted by a signal then
  // resume it. If the runner is stopped (as WUNTRACED isn't specified then this
  // will only happen if the runner is ptraced) then continue to wait on it.
  do {
    e = waitpid(runner, &status, 0);
  } while ((
    e == -1 && errno == EINTR
  ) && (
    !WIFSTOPPED(status) || !WIFSIGNALED(status)
  ));

  if (WIFSIGNALED(status))
    return false;

  if (WIFEXITED(status))
    return WEXITSTATUS(status) == EXIT_SUCCESS;

  fprintf(stderr, "unable to get status of test runner of %s\n", test->name);
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  if (v_test == NULL)
    return EXIT_SUCCESS;

  bool fail = false;

  for (size_t i = 0; i < mx_vector_length(v_test); i++) {
    test_t *test = v_test + i;

    fprintf(stderr, "%s:%zu:%s():", test->file, test->line, test->name);

    if (test_execute(test)) {
      fprintf(stderr, " [PASSED]\n");
    } else {
      fprintf(stderr, " [FAILED]\n");
      fail = true;
    }
  }

  mx_vector_delete(v_test);

  return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}
