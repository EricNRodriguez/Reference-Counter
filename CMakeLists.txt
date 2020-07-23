cmake_minimum_required(VERSION 3.15)
project(rc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS_DEBUG "-D_DEBUG")
add_library(rc rc.c rc.h)