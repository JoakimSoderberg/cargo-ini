cmake_minimum_required (VERSION 2.8)
project(cargo_ini C)

include_directories(cargo/ alini/)
add_executable(cargo_ini cargo/cargo.c alini/alini.c cargo_ini.c cargo_ini_example.c)

