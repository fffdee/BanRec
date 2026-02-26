cmake_minimum_required(VERSION 3.10)
project(BanRec C)

# ============================================
# 编译器设置
# ============================================
set(CMAKE_C_COMPILER gcc)
set(CMAKE_C_STANDARD 11)

set(PLATFORM_LIBS ev asound m)

set(PLATFORM "linux" CACHE STRING "Target platform")


# ============================================
# 编译选项
# ============================================
add_compile_options(-Wall -O2)

# ============================================
# 头文件目录
# ============================================
set(INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/hal/input_handle/
    ${CMAKE_SOURCE_DIR}/hal/play/

    ${CMAKE_SOURCE_DIR}/core/
    ${CMAKE_SOURCE_DIR}/interface/
    ${CMAKE_SOURCE_DIR}/api/
    ${CMAKE_SOURCE_DIR}/
)

# ============================================
# 核心源文件
# ============================================
set(CORE_SRC
    ${CMAKE_SOURCE_DIR}/main.c
    ${CMAKE_SOURCE_DIR}/core/rec_core.c
    ${CMAKE_SOURCE_DIR}/core/rec_err_handle.c
)

# ============================================
# API层源文件
# ============================================
set(API_SRC
    ${CMAKE_SOURCE_DIR}/api/rec_api.c
    ${CMAKE_SOURCE_DIR}/api/rec_test.c
)

# ============================================
# 接口层源文件
# ============================================
set(INTERFACE_SRC
    ${CMAKE_SOURCE_DIR}/interface/rec_interface.c
)

# ============================================
# HAL层源文件
# ============================================
set(HAL_LAYER_SRC

    ${CMAKE_SOURCE_DIR}/hal/play/play.c
    ${CMAKE_SOURCE_DIR}/hal/input_handle/input_handle.c
)

# ============================================
# 生成可执行文件
# ============================================
set(ALL_SRC ${CORE_SRC} ${API_SRC} ${INTERFACE_SRC} ${HAL_LAYER_SRC})

add_executable(${PROJECT_NAME} ${ALL_SRC})

# ============================================
# 设置头文件目录
# ============================================
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIRS})

# ============================================
# 链接库
# ============================================
target_link_libraries(${PROJECT_NAME} ${PLATFORM_LIBS})

# ============================================
# 编译信息
# ============================================
message(STATUS "========================================")
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "Platform: ${PLATFORM}")
message(STATUS "Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "========================================")
