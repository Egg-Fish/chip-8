cmake_minimum_required(VERSION 3.16)
project(CHIP-8)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(
    CHIP-8
    src/CHIP-8.c
)