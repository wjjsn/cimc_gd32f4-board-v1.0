cmake_minimum_required(VERSION 3.16)
project(schedule VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(schedule schedule.cpp)

# 提供与 meson.override_dependency(template_schedule) 等效的接口目标
add_library(template_schedule INTERFACE)
target_include_directories(template_schedule INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(schedule PRIVATE template_schedule)

install(TARGETS schedule RUNTIME DESTINATION bin)

enable_testing()
add_test(NAME basic COMMAND schedule)