# 反射代码生成CMake模块
# 使用方法: include(${CMAKE_CURRENT_LIST_DIR}/reflection.cmake)
#           generate_reflection(TARGET <目标名> HEADERS <头文件列表>)

set(RTTM_SYSTEM_PATHS "" CACHE STRING "系统库路径列表，用于反射生成时排除")

get_filename_component(REFLECTION_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" ABSOLUTE)
get_filename_component(REFLECTION_CMAKE_DIR "${REFLECTION_CMAKE_DIR}" DIRECTORY)

find_package(Python3 COMPONENTS Interpreter QUIET)

if(NOT Python3_FOUND)
    message(STATUS "未检测到Python环境，请安装Python 3.x")
    return()
else()
    message(STATUS "检测到Python: ${Python3_EXECUTABLE} (${Python3_VERSION})")
endif()

set(GENERATOR_SCRIPT "${REFLECTION_CMAKE_DIR}/generate_reflection.py")

execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import clang.cindex; print('FOUND')"
        OUTPUT_VARIABLE CLANG_CHECK_OUTPUT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT CLANG_CHECK_OUTPUT STREQUAL "FOUND")
    message(STATUS "正在安装Python libclang库...")
    execute_process(
            COMMAND ${Python3_EXECUTABLE} -m pip install libclang
            RESULT_VARIABLE PIP_RESULT
    )

    if(NOT PIP_RESULT EQUAL 0)
        message(WARNING "无法安装libclang库。请手动运行: pip install libclang")
    else()
        message(STATUS "libclang库安装成功")
    endif()
endif()

# 定义全局属性，用于跟踪已处理的头文件
define_property(GLOBAL PROPERTY RTTM_PROCESSED_HEADERS
        BRIEF_DOCS "已经处理过的头文件列表，防止重复生成反射代码"
        FULL_DOCS "保存所有已经为其生成反射代码的头文件的绝对路径，用于防止重复生成"
)
# 初始化已处理头文件列表
set_property(GLOBAL PROPERTY RTTM_PROCESSED_HEADERS "")

function(get_target_include_dirs TARGET OUTPUT_VAR)
    # 获取目标的包含目录
    set(INCLUDE_DIRS "")

    # 获取目标的直接包含目录
    get_target_property(TARGET_INCLUDE_DIRS ${TARGET} INCLUDE_DIRECTORIES)
    if(TARGET_INCLUDE_DIRS)
        list(APPEND INCLUDE_DIRS ${TARGET_INCLUDE_DIRS})
    endif()

    # 获取目标的接口包含目录
    get_target_property(TARGET_INTERFACE_INCLUDE_DIRS ${TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    if(TARGET_INTERFACE_INCLUDE_DIRS)
        list(APPEND INCLUDE_DIRS ${TARGET_INTERFACE_INCLUDE_DIRS})
    endif()

    # 获取所有链接库的包含目录
    get_target_property(TARGET_LINK_LIBRARIES ${TARGET} LINK_LIBRARIES)
    if(TARGET_LINK_LIBRARIES)
        foreach(LIB ${TARGET_LINK_LIBRARIES})
            if(TARGET ${LIB})
                get_target_property(LIB_INTERFACE_INCLUDE_DIRS ${LIB} INTERFACE_INCLUDE_DIRECTORIES)
                if(LIB_INTERFACE_INCLUDE_DIRS)
                    list(APPEND INCLUDE_DIRS ${LIB_INTERFACE_INCLUDE_DIRS})
                endif()
            endif()
        endforeach()
    endif()

    # 移除重复项并设置输出变量
    if(INCLUDE_DIRS)
        list(REMOVE_DUPLICATES INCLUDE_DIRS)
    endif()

    set(${OUTPUT_VAR} "${INCLUDE_DIRS}" PARENT_SCOPE)
endfunction()

# 生成头文件列表文件
function(generate_headers_list_file OUTPUT_FILE HEADERS)
    file(WRITE "${OUTPUT_FILE}" "")
    foreach(HEADER ${HEADERS})
        get_filename_component(HEADER_ABS "${HEADER}" ABSOLUTE)
        file(APPEND "${OUTPUT_FILE}" "${HEADER_ABS}\n")
    endforeach()
endfunction()

function(generate_reflection)
    cmake_parse_arguments(REFL "" "TARGET" "HEADERS;EXCLUDE_PATHS" ${ARGN})

    if(NOT DEFINED REFL_TARGET)
        message(FATAL_ERROR "必须指定目标: generate_reflection(TARGET <目标名> ...)")
    endif()

    if(NOT DEFINED REFL_HEADERS OR "${REFL_HEADERS}" STREQUAL "")
        message(STATUS "未指定头文件: generate_reflection(... HEADERS <头文件列表>) - 跳过反射生成")
        return()
    endif()

    # 检查目标是否存在
    if(NOT TARGET ${REFL_TARGET})
        message(WARNING "目标 ${REFL_TARGET} 不存在，跳过反射生成")
        return()
    endif()

    set(REFLECTION_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated_reflection")
    file(MAKE_DIRECTORY "${REFLECTION_OUTPUT_DIR}")

    # 生成编译选项文件
    set(COMPILE_OPTIONS_FILE "${REFLECTION_OUTPUT_DIR}/${REFL_TARGET}_compile_options.json")

    # 获取目标的包含目录
    get_target_include_dirs(${REFL_TARGET} TARGET_ALL_INCLUDE_DIRS)

    # 将包含目录写入编译选项文件
    file(WRITE ${COMPILE_OPTIONS_FILE} "{\n  \"options\": [\n")
    set(FIRST_ENTRY TRUE)
    foreach(INCLUDE_DIR ${TARGET_ALL_INCLUDE_DIRS})
        # 跳过生成器表达式
        if(NOT "${INCLUDE_DIR}" MATCHES "\\$<")
            if(FIRST_ENTRY)
                set(FIRST_ENTRY FALSE)
            else()
                file(APPEND ${COMPILE_OPTIONS_FILE} ",\n")
            endif()
            file(APPEND ${COMPILE_OPTIONS_FILE} "    \"-I${INCLUDE_DIR}\"")
        endif()
    endforeach()
    file(APPEND ${COMPILE_OPTIONS_FILE} "\n  ]\n}\n")

    # 将所有包含目录转换为逗号分隔的字符串
    string(REPLACE ";" "," INCLUDE_PATHS_STR "${TARGET_ALL_INCLUDE_DIRS}")

    # 获取已处理过的头文件列表
    get_property(PROCESSED_HEADERS GLOBAL PROPERTY RTTM_PROCESSED_HEADERS)

    # 过滤出未处理的头文件
    set(UNPROCESSED_HEADERS "")
    foreach(HEADER ${REFL_HEADERS})
        get_filename_component(HEADER_ABS "${HEADER}" ABSOLUTE)
        list(FIND PROCESSED_HEADERS "${HEADER_ABS}" HEADER_INDEX)
        if(HEADER_INDEX GREATER -1)
            message(STATUS "跳过已处理的头文件: ${HEADER}")
        else()
            list(APPEND UNPROCESSED_HEADERS "${HEADER_ABS}")
        endif()
    endforeach()

    # 如果没有未处理的头文件，直接返回
    if(NOT UNPROCESSED_HEADERS)
        message(STATUS "目标 ${REFL_TARGET} 没有新的头文件需要生成反射代码")
        return()
    endif()

    # 更新已处理头文件列表
    set_property(GLOBAL APPEND PROPERTY RTTM_PROCESSED_HEADERS ${UNPROCESSED_HEADERS})

    # 生成单一反射文件
    set(REFLECTION_FILE "${REFLECTION_OUTPUT_DIR}/${REFL_TARGET}_reflection.cpp")

    # 生成头文件列表文件
    set(HEADERS_LIST_FILE "${REFLECTION_OUTPUT_DIR}/${REFL_TARGET}_headers.txt")
    generate_headers_list_file(${HEADERS_LIST_FILE} "${UNPROCESSED_HEADERS}")

    # 创建自定义命令，一次性处理所有头文件
    add_custom_command(
            OUTPUT "${REFLECTION_FILE}"
            COMMAND ${Python3_EXECUTABLE} "${GENERATOR_SCRIPT}"
            "--headers-list=${HEADERS_LIST_FILE}"
            "--output=${REFLECTION_FILE}"
            "--options=${COMPILE_OPTIONS_FILE}"
            "--include-paths=${INCLUDE_PATHS_STR}"
            DEPENDS "${HEADERS_LIST_FILE}" "${GENERATOR_SCRIPT}" "${COMPILE_OPTIONS_FILE}" ${UNPROCESSED_HEADERS}
            COMMENT "为目标 ${REFL_TARGET} 生成合并的反射代码"
            VERBATIM
    )
    message("命令:" ${Python3_EXECUTABLE} " ${GENERATOR_SCRIPT}"
            " --headers-list=${HEADERS_LIST_FILE}"
            " --output=${REFLECTION_FILE}"
            " --options=${COMPILE_OPTIONS_FILE}"
            " --include-paths=${INCLUDE_PATHS_STR}")

    # 将生成的反射代码文件直接添加到原始目标中
    target_sources(${REFL_TARGET} PRIVATE ${REFLECTION_FILE})

    message(STATUS "成功添加反射代码到目标 ${REFL_TARGET} (包含 ${UNPROCESSED_HEADERS} 个头文件)")
endfunction()

function(rttm_add_reflection TARGET)
    # 检查目标是否存在
    if(NOT TARGET ${TARGET})
        message(WARNING "目标 ${TARGET} 不存在，跳过反射生成")
        return()
    endif()

    # 检查目标类型，确保是库或可执行文件
    get_target_property(TARGET_TYPE ${TARGET} TYPE)
    if(NOT (TARGET_TYPE STREQUAL "EXECUTABLE" OR
            TARGET_TYPE STREQUAL "STATIC_LIBRARY" OR
            TARGET_TYPE STREQUAL "SHARED_LIBRARY" OR
            TARGET_TYPE STREQUAL "MODULE_LIBRARY" OR
            TARGET_TYPE STREQUAL "OBJECT_LIBRARY"))
        message(WARNING "目标 ${TARGET} 类型 (${TARGET_TYPE}) 不支持反射生成")
        return()
    endif()

    # 检查是否链接了RTTM库
    get_target_property(TARGET_LIBS ${TARGET} LINK_LIBRARIES)
    set(LINKED_TO_RTTM FALSE)

    if(TARGET_LIBS)
        foreach(LIB ${TARGET_LIBS})
            if(LIB MATCHES "RTTM")
                set(LINKED_TO_RTTM TRUE)
                break()
            endif()
        endforeach()
    endif()

    if(NOT LINKED_TO_RTTM)
        message(WARNING "目标 ${TARGET} 未链接RTTM库，跳过反射生成")
        return()
    endif()

    get_target_property(TARGET_SOURCES ${TARGET} SOURCES)

    set(HEADER_FILES "")
    foreach(SOURCE ${TARGET_SOURCES})
        if(SOURCE MATCHES "\\.(cpp|cxx|cc)$")
            get_filename_component(SOURCE_NAME "${SOURCE}" NAME_WE)

            foreach(EXT h hpp hxx)
                set(HEADER_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${SOURCE_NAME}.${EXT}")
                if(EXISTS "${HEADER_FILE}")
                    list(APPEND HEADER_FILES "${HEADER_FILE}")
                    break()
                endif()
            endforeach()
        elseif(SOURCE MATCHES "\\.(h|hpp|hxx)$")
            list(APPEND HEADER_FILES "${SOURCE}")
        endif()
    endforeach()

    if(HEADER_FILES)
        message(STATUS "为目标 ${TARGET} 生成反射代码，扫描到的头文件：")
        foreach(HEADER ${HEADER_FILES})
            message(STATUS "  - ${HEADER}")
        endforeach()

        generate_reflection(
                TARGET ${TARGET}
                HEADERS ${HEADER_FILES}
        )
    else()
        message(STATUS "目标 ${TARGET} 没有找到头文件，跳过反射生成")
    endif()
endfunction()