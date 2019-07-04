cmake_minimum_required(VERSION 3.0)

project("Embedded DB"
  VERSION 0.0.1
  HOMEPAGE_URL https://github.com/TheBeehive/embeddeddb
  LANGUAGES C)

set(CMAKE_C_STANDARD 11)

add_executable(embeddeddb
  source/database.c
  source/database.h
  source/main.c)
