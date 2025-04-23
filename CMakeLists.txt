cmake_minimum_required(VERSION 3.15)
project(RTTM)

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

add_library(RTTM_static STATIC ${SRC_FILES} ${HEAD_FILES})
target_link_libraries(RTTM_static PUBLIC
        ${ANDROID_LIBS}
)

set_target_properties(RTTM_static PROPERTIES
        OUTPUT_NAME "RTTM_static"
        ARCHIVE_OUTPUT_DIRECTORY ${STATIC_OUTPUT_DIR}
)
target_compile_definitions(RTTM_static PRIVATE "RTTM_STATIC")
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