cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED)
project(l2_task1)

target_sources(app PRIVATE src/main.c)
