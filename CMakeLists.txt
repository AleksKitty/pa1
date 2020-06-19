cmake_minimum_required(VERSION 3.16)
project(pa1 C)

set(CMAKE_C_STANDARD 99)
add_compile_options(-Wall -pedantic)
add_executable(pa1 main.c ipc.h common.h ipc.c banking.h pa23.c bank_robbery.c print_history.c pa2345.h)

find_library(libruntime libruntime.so paths /home/alex/Desktop/pa2/lib64/)
target_link_libraries(pa1 ${libruntime})