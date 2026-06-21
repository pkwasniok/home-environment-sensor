cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED)

project(app LANGUAGES C)

target_sources(app PRIVATE src/main.c src/bmp280.c src/pms5003.c src/led.c)

