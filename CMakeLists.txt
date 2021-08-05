cmake_minimum_required(VERSION 3.16)

project (status)
add_definitions("
-pipe 
-std=c++20 -Wall -Wextra -Wshadow
-Wnon-virtual-dtor
-Wold-style-cast
-Wcast-align
-Wunused
-Woverloaded-virtual
-Wpedantic
-Wconversion
-Wsign-conversion
-Wnull-dereference
-Wdouble-promotion
-Wformat=2
-Wduplicated-cond
-Wduplicated-branches
-Wlogical-op
-Wuseless-cast
-Winit-list-lifetime
-fno-asynchronous-unwind-tables
-fanalyzer
-march=native -Os  
") 

add_executable(status status.cpp ${SOURCES})

find_package(PkgConfig REQUIRED)
pkg_check_modules(deps REQUIRED IMPORTED_TARGET x11 libmpdclient fmt)
target_link_libraries(status PkgConfig::deps)
