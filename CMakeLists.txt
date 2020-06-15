cmake_minimum_required(VERSION 3.16)
project(pa1 C)

set(CMAKE_C_STANDARD 99)

add_executable(pa1 main.c ipc.h common.h pa1.h ipc.c)