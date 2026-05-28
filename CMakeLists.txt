# Copyright 2026 Jan-Willem Smaal <usenet@gispen.org>
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(KCONFIG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/Kconfig)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(i2s_generator)

target_sources(app PRIVATE 
    src/main.c 
    src/generators.c
    src/shell.c
)

target_include_directories(app PRIVATE src)
