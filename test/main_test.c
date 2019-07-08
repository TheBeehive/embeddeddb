#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "test.h"

#include "../source/database.h"

// when `/tmp/example` doesn't exist then it creates it and returns a usable
// database
TEST(database_new_normal) {
  assert(unlink("/tmp/example") == 0 || errno == ENOENT);

  database_t *database = database_new("/tmp/example");

  assert(access("/tmp/example", F_OK) != -1);
  assert(database != NULL);
}

// when `/tmp/example` exists and is readable then it returns a usable database
TEST(database_new_readable) {
  assert(unlink("/tmp/example") == 0 || errno == ENOENT);
  int fd = open("/tmp/example", O_RDWR | O_CREAT | O_EXCL, 0644);
  close(fd);

  database_t *database = database_new("/tmp/example");
  assert(database != NULL);
}

// when `/tmp/example` exists and isn't readable then it returns NULL
TEST(database_new_unreadable) {
  assert(unlink("/tmp/example") == 0 || errno == ENOENT);
  int fd = open("/tmp/example", O_RDWR | O_CREAT | O_EXCL, 0000);
  close(fd);

  database_t *database = database_new("/tmp/example");
  assert(database == NULL);
}
