cmake_minimum_required(VERSION 3.20)
project(RTTM VERSION 2.0.0 LANGUAGES CXX)

# C++20 is required for RTTM
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options for C++20 support
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
        message(FATAL_ERROR "GCC 10.0 or later is required for C++20 support")
    endif()
    add_compile_options(-fconcepts)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "10.0")
        message(FATAL_ERROR "Clang 10.0 or later is required for C++20 support")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "19.29")
        message(FATAL_ERROR "MSVC 19.29 (VS 2019 16.10) or later is required for C++20 support")
    endif()
    add_compile_options(/Zc:__cplusplus)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflection.cmake)

file(GLOB_RECURSE SRC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE HEAD_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SHARED_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/shared")
set(STATIC_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}/static")

if (ANDROID)
    set(ANDROID_NDK_HOME "")
    set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
    set(VCPKG_TARGET_TRIPLET "arm64-android")
    set(ANDROID_LIBS log android)
endif ()

# Static library target
add_library(RTTM_static STATIC ${SRC_FILES} ${HEAD_FILES})
target_compile_features(RTTM_static PUBLIC cxx_std_20)
target_link_libraries(RTTM_static PUBLIC ${ANDROID_LIBS})

set_target_properties(RTTM_static PROPERTIES
    OUTPUT_NAME "RTTM_static"
    ARCHIVE_OUTPUT_DIRECTORY ${STATIC_OUTPUT_DIR}
)

target_compile_definitions(RTTM_static PRIVATE "RTTM_STATIC")
target_compile_definitions(RTTM_static PUBLIC "RTTM_CXX20_AVAILABLE")

target_include_directories(RTTM_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Install targets
install(TARGETS RTTM_static
    RUNTIME DESTINATION ${CMAKE_BINARY_DIR}
    LIBRARY DESTINATION ${CMAKE_BINARY_DIR}
    ARCHIVE DESTINATION ${CMAKE_BINARY_DIR}
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/RTTM DESTINATION ${CMAKE_BINARY_DIR}/include)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/cmake/reflection.cmake DESTINATION ${CMAKE_BINARY_DIR}/cmake)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generate_reflection.py
    DESTINATION ${CMAKE_BINARY_DIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# Interface library alias
add_library(RTTM INTERFACE)
target_link_libraries(RTTM INTERFACE RTTM_static)
add_library(RTTM::RTTM ALIAS RTTM_static)

# Example executable
add_executable(basic_usage_example examples/basic_usage.cpp)
target_link_libraries(basic_usage_example PRIVATE RTTM_static)
target_compile_features(basic_usage_example PRIVATE cxx_std_20)

# ============================================================================
# Benchmarks (requires Google Benchmark)
# ============================================================================

find_package(benchmark CONFIG QUIET)

if(benchmark_FOUND)
    # RTTM Benchmark
    add_executable(rttm_benchmark benchmark/rttm_benchmark.cpp)
    target_link_libraries(rttm_benchmark PRIVATE 
        RTTM_static 
        benchmark::benchmark 
        benchmark::benchmark_main
    )
    target_compile_features(rttm_benchmark PRIVATE cxx_std_20)
    message(STATUS "RTTM benchmark enabled")
    
    # RTTR Benchmark (requires RTTR library)
    find_package(rttr CONFIG QUIET)
    if(rttr_FOUND)
        add_executable(rttr_benchmark benchmark/rttr_benchmark.cpp)
        target_link_libraries(rttr_benchmark PRIVATE 
            benchmark::benchmark 
            benchmark::benchmark_main
            RTTR::Core
        )
        # RTTR requires C++17
        set_target_properties(rttr_benchmark PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
        )
        message(STATUS "RTTR benchmark enabled")
    else()
        message(STATUS "RTTR benchmark disabled (rttr not found)")
    endif()
else()
    message(STATUS "Benchmarks disabled (Google Benchmark not found)")
endif()

# Build targets
add_custom_target(build_all ALL DEPENDS RTTM_static)
add_custom_target(install_all
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}
    DEPENDS build_all
)
add_custom_target(default ALL DEPENDS install_all)
