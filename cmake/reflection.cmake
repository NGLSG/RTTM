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

# 获取目标所有链接库的函数
function(get_target_all_link_libraries TARGET OUTPUT_VAR)
    # 初始化链接库列表
    set(ALL_LIBS "")

    # 获取目标的直接链接库
    get_target_property(TARGET_LIBS ${TARGET} LINK_LIBRARIES)
    if(TARGET_LIBS)
        list(APPEND ALL_LIBS ${TARGET_LIBS})
    endif()

    # 获取目标的接口链接库
    get_target_property(TARGET_INTERFACE_LIBS ${TARGET} INTERFACE_LINK_LIBRARIES)
    if(TARGET_INTERFACE_LIBS)
        list(APPEND ALL_LIBS ${TARGET_INTERFACE_LIBS})
    endif()

    # 移除重复项
    if(ALL_LIBS)
        list(REMOVE_DUPLICATES ALL_LIBS)
    endif()

    # 设置输出变量
    set(${OUTPUT_VAR} "${ALL_LIBS}" PARENT_SCOPE)
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

    set(GENERATED_FILES "")
    set(NEW_PROCESSED_HEADERS "")

    foreach(HEADER ${REFL_HEADERS})
        get_filename_component(HEADER_ABS "${HEADER}" ABSOLUTE)
        get_filename_component(HEADER_NAME "${HEADER}" NAME_WE)

        # 检查该头文件是否已经处理过
        list(FIND PROCESSED_HEADERS "${HEADER_ABS}" HEADER_INDEX)
        if(HEADER_INDEX GREATER -1)
            message(STATUS "跳过已处理的头文件: ${HEADER}")
            continue()
        endif()

        # 记录新处理的头文件
        list(APPEND NEW_PROCESSED_HEADERS "${HEADER_ABS}")

        set(REFLECTION_FILE "${REFLECTION_OUTPUT_DIR}/${REFL_TARGET}_${HEADER_NAME}_reflection.cpp")
        list(APPEND GENERATED_FILES "${REFLECTION_FILE}")

        add_custom_command(
                OUTPUT "${REFLECTION_FILE}"
                COMMAND ${Python3_EXECUTABLE} "${GENERATOR_SCRIPT}" "${HEADER_ABS}" "${REFLECTION_FILE}" "${COMPILE_OPTIONS_FILE}" "${INCLUDE_PATHS_STR}"
                DEPENDS "${HEADER_ABS}" "${GENERATOR_SCRIPT}" "${COMPILE_OPTIONS_FILE}"
                COMMENT "为 ${HEADER} 生成反射代码"
                VERBATIM
        )
    endforeach()

    # 更新全局已处理头文件列表
    if(NEW_PROCESSED_HEADERS)
        set_property(GLOBAL APPEND PROPERTY RTTM_PROCESSED_HEADERS ${NEW_PROCESSED_HEADERS})
    endif()

    if(GENERATED_FILES)
        # 为反射代码创建一个对象库
        add_library(${REFL_TARGET}_reflection OBJECT ${GENERATED_FILES})

        # 设置和目标相同的包含目录
        target_include_directories(${REFL_TARGET}_reflection PRIVATE ${TARGET_ALL_INCLUDE_DIRS})

        # 添加生成的反射代码目录
        target_include_directories(${REFL_TARGET}_reflection PRIVATE ${REFLECTION_OUTPUT_DIR})

        # 获取原目标的所有链接库并应用到反射目标
        get_target_all_link_libraries(${REFL_TARGET} TARGET_ALL_LINK_LIBS)

        # 确保至少链接RTTM库
        if(NOT TARGET_ALL_LINK_LIBS)
            target_link_libraries(${REFL_TARGET}_reflection PRIVATE RTTM)
        else()
            # 将原始目标的所有链接库都链接到反射目标
            # 但要避免链接自身，防止循环依赖
            set(FILTERED_LINK_LIBS "")
            foreach(LIB ${TARGET_ALL_LINK_LIBS})
                if(NOT "${LIB}" STREQUAL "${REFL_TARGET}")
                    list(APPEND FILTERED_LINK_LIBS ${LIB})
                endif()
            endforeach()

            # 链接所有过滤后的库
            target_link_libraries(${REFL_TARGET}_reflection PRIVATE ${FILTERED_LINK_LIBS})

            # 确保RTTM被链接
            target_link_libraries(${REFL_TARGET}_reflection PRIVATE RTTM)
        endif()

        # 获取原目标的编译定义并应用到反射目标
        get_target_property(TARGET_COMPILE_DEFS ${REFL_TARGET} COMPILE_DEFINITIONS)
        if(TARGET_COMPILE_DEFS)
            target_compile_definitions(${REFL_TARGET}_reflection PRIVATE ${TARGET_COMPILE_DEFS})
        endif()

        # 获取原目标的编译选项并应用到反射目标
        get_target_property(TARGET_COMPILE_OPTIONS ${REFL_TARGET} COMPILE_OPTIONS)
        if(TARGET_COMPILE_OPTIONS)
            target_compile_options(${REFL_TARGET}_reflection PRIVATE ${TARGET_COMPILE_OPTIONS})
        endif()

        # 获取原目标的编译特性并应用到反射目标
        get_target_property(TARGET_COMPILE_FEATURES ${REFL_TARGET} COMPILE_FEATURES)
        if(TARGET_COMPILE_FEATURES)
            target_compile_features(${REFL_TARGET}_reflection PRIVATE ${TARGET_COMPILE_FEATURES})
        endif()

        # 重要：禁用反射目标的预编译头功能，避免冲突
        set_target_properties(${REFL_TARGET}_reflection PROPERTIES
                DISABLE_PRECOMPILE_HEADERS ON
        )

        # 将反射库链接到主目标
        target_link_libraries(${REFL_TARGET} PRIVATE ${REFL_TARGET}_reflection)

        message(STATUS "成功创建反射目标: ${REFL_TARGET}_reflection (${REFL_TARGET}的${GENERATED_FILES}个文件)")
    else()
        message(STATUS "目标 ${REFL_TARGET} 没有新的头文件需要生成反射代码")
    endif()
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

    # 检查这个目标是否已经有反射库
    if(TARGET ${TARGET}_reflection)
        message(STATUS "目标 ${TARGET} 已经有反射库，跳过重复生成")
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