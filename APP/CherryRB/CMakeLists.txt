cmake_minimum_required(VERSION 3.22)
enable_language(C CXX)

add_library(CherryRB OBJECT)
target_sources(CherryRB PRIVATE "${CMAKE_CURRENT_LIST_DIR}/chry_ringbuffer.c")
target_include_directories(CherryRB PUBLIC "${CMAKE_CURRENT_LIST_DIR}")

# target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CherryRB)
target_link_libraries(${CMAKE_PROJECT_NAME} CherryRB)