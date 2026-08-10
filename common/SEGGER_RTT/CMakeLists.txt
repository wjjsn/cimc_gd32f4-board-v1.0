cmake_minimum_required(VERSION 3.22)
enable_language(C ASM)

add_library(SEGGER_RTT OBJECT)
target_sources(SEGGER_RTT PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/SEGGER_RTT.c"
    "${CMAKE_CURRENT_LIST_DIR}/SEGGER_RTT_printf.c"
    "${CMAKE_CURRENT_LIST_DIR}/SEGGER_RTT_ASM_ARMv7M.S"#如果不需要，就注释掉这行
    # "${CMAKE_CURRENT_LIST_DIR}/RTT_PrintfTest.c"#如果想要执行测试函数，解除注释这行
)
target_include_directories(SEGGER_RTT PUBLIC "${CMAKE_CURRENT_LIST_DIR}")
# target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE SEGGER_RTT)
target_link_libraries(${CMAKE_PROJECT_NAME} SEGGER_RTT)#如果需要无关键字签名，使用这行