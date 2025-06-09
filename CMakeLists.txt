cmake_minimum_required(VERSION 3.15)
project(RTTM)

# 设置默认的C++标准为17
set(CMAKE_CXX_STANDARD 17)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflection.cmake)

file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE HEAD_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/include/RTTM)

set(SHARED_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/shared")
set(STATIC_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/static")

if (ANDROID)
    set(ANDROID_NDK_HOME "")
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
    set(VCPKG_TARGET_TRIPLET "arm64-android")
    set(ANDROID_LIBS log android)
endif ()

# 创建静态库目标
add_library(RTTM_static STATIC ${SRC_FILES} ${HEAD_FILES})

# 为整个目标设置C++17标准
target_compile_features(RTTM_static PUBLIC cxx_std_17)

# 检查编译器是否支持C++20
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++20" COMPILER_SUPPORTS_CXX20)

if(COMPILER_SUPPORTS_CXX20)
    # 为包含RTTM.hpp的源文件单独设置C++20标准
    set_source_files_properties(
            ${CMAKE_CURRENT_SOURCE_DIR}/include/RTTM.hpp
            PROPERTIES
            COMPILE_FLAGS "-std=c++20"
    )

    # 如果有特定的源文件需要使用RTTM.hpp，也需要设置为C++20
    file(GLOB RTTM_CORE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/RTTM*.cpp")
    if(RTTM_CORE_FILES)
        set_source_files_properties(
                ${RTTM_CORE_FILES}
                PROPERTIES
                COMPILE_FLAGS "-std=c++20"
        )
    endif()

    message(STATUS "RTTM.hpp将使用C++20标准编译")
else()
    message(WARNING "编译器不支持C++20，RTTM.hpp将回退到C++17")
endif()

target_link_libraries(RTTM_static PUBLIC
        ${ANDROID_LIBS}
)

set_target_properties(RTTM_static PROPERTIES
        OUTPUT_NAME "RTTM_static"
        ARCHIVE_OUTPUT_DIRECTORY ${STATIC_OUTPUT_DIR}
)

# 设置编译宏定义
target_compile_definitions(RTTM_static PRIVATE "RTTM_STATIC")

# 根据编译器支持情况设置条件编译宏
if(COMPILER_SUPPORTS_CXX20)
    target_compile_definitions(RTTM_static PRIVATE "RTTM_CXX20_AVAILABLE")
endif()

target_include_directories(RTTM_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

install(TARGETS RTTM_static
        RUNTIME DESTINATION ${CMAKE_BINARY_DIR}
        LIBRARY DESTINATION ${CMAKE_BINARY_DIR}
        ARCHIVE DESTINATION ${CMAKE_BINARY_DIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/RTTM DESTINATION ${CMAKE_BINARY_DIR}/include)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflection.cmake
        DESTINATION ${CMAKE_BINARY_DIR}/cmake)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generate_reflection.py
        DESTINATION ${CMAKE_BINARY_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)

add_library(RTTM INTERFACE)
target_link_libraries(RTTM INTERFACE RTTM_static)
add_library(RTTM::RTTM ALIAS RTTM_static)

add_custom_target(build_all ALL DEPENDS RTTM_static)
add_custom_target(install_all
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}
        DEPENDS build_all
)
add_custom_target(default ALL DEPENDS install_all)