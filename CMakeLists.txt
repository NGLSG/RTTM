project(RTTM)
set(CMAKE_CXX_STANDARD 17)
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

add_library(RTTM_shared SHARED ${SRC_FILES} ${HEAD_FILES})
target_link_libraries(RTTM_shared PUBLIC
        ${ANDROID_LIBS}
)
# 设置动态库的输出文件名和目录
set_target_properties(RTTM_shared PROPERTIES
        OUTPUT_NAME "RTTM_shared"
        LIBRARY_OUTPUT_DIRECTORY ${SHARED_OUTPUT_DIR}
)

# 创建静态库目标
add_library(RTTM_static STATIC ${SRC_FILES} ${HEAD_FILES})
target_link_libraries(RTTM_static PUBLIC
        ${ANDROID_LIBS}
)
# 设置静态库的输出文件名和目录
set_target_properties(RTTM_static PROPERTIES
        OUTPUT_NAME "RTTM_static"
        ARCHIVE_OUTPUT_DIRECTORY ${STATIC_OUTPUT_DIR}
)

target_include_directories(RTTM_shared PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(RTTM_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

install(TARGETS RTTM_shared RTTM_static
        RUNTIME DESTINATION ${CMAKE_BINARY_DIR}
        LIBRARY DESTINATION ${CMAKE_BINARY_DIR}
        ARCHIVE DESTINATION ${CMAKE_BINARY_DIR})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/RTTM DESTINATION ${CMAKE_BINARY_DIR}/include)

add_library(RTTM::RTTM ALIAS RTTM_static)
add_custom_target(build_all ALL DEPENDS RTTM_shared RTTM_static)

# Add an install target that depends on build_all
add_custom_target(install_all
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}
        DEPENDS build_all
)

# Make install_all the default target
add_custom_target(default ALL DEPENDS install_all)