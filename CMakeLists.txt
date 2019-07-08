cmake_minimum_required(VERSION 3.12)

project("Embedded DB"
  VERSION 0.0.1
  HOMEPAGE_URL https://github.com/TheBeehive/embeddeddb
  LANGUAGES C)

include(CTest)

set(CMAKE_C_STANDARD 11)

add_executable(embeddeddb
  source/database.c
  source/database.h
  source/main.c)

add_executable(main_test
  source/database.c
  source/database.h
  test/mx/common.c
  test/mx/common.h
  test/mx/vector.c
  test/mx/vector.h
  test/test.h
  test/test.c
  test/main_test.c)

add_test(main_test main_test)
