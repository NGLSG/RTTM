#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 使用clang库解析C++代码并生成反射代码

import sys
import os
import re
import argparse
import clang.cindex
from clang.cindex import CursorKind, TypeKind, StorageClass
import json
import tempfile
from collections import defaultdict


class ReflectionGenerator:
    def __init__(self, output_file, compile_options_file=None, include_paths=None):
        """初始化反射代码生成器"""
        self.output_file = output_file
        self.compile_options_file = compile_options_file
        self.include_paths = include_paths or []
        self.index = clang.cindex.Index.create()

        # 按命名空间组织的注册代码
        self.namespace_registrations = defaultdict(list)

        # 记录已处理的源文件
        self.processed_files = set()

        # 记录无法解析的外部类型
        self.external_types = set()

        # 记录成功解析的类型
        self.resolved_types = set()

        # 记录跳过的类型
        self.skipped_types = []

        # 记录已注册的类型，避免重复
        self.registered_types = set()

        # 存储所有类型的信息字典，用于后续处理
        self.type_infos = {}

        # 跟踪当前的访问修饰符状态
        self.current_access_state = {}

    def load_compile_options(self):
        """从编译选项文件加载选项"""
        # 基础选项
        options = ['-x', 'c++', '-std=c++17']

        # 特殊选项：忽略不会找到的外部库
        options.append('-Wno-pragma-once-outside-header')
        options.append('-Wno-unknown-warning-option')
        options.append('-Wno-unknown-pragmas')
        options.append('-Wno-ignored-attributes')

        # 添加包含路径
        for path in self.include_paths:
            if path:
                options.append(f'-I{path}')


        if self.compile_options_file and os.path.exists(self.compile_options_file):
            try:
                # 首先尝试读取JSON格式
                with open(self.compile_options_file, 'r') as f:
                    try:
                        data = json.load(f)
                        if isinstance(data, list):
                            for item in data:
                                if isinstance(item, str) and not item.startswith('#'):
                                    options.append(item)
                        elif isinstance(data, dict) and 'options' in data:
                            for item in data['options']:
                                if isinstance(item, str) and not item.startswith('#'):
                                    options.append(item)
                        print(f"从JSON文件 {self.compile_options_file} 加载了编译选项")
                    except json.JSONDecodeError:
                        # 不是JSON格式，回退到按行读取
                        f.seek(0)
                        for line in f:
                            line = line.strip()
                            if line and not line.startswith('#'):  # 忽略注释
                                options.append(line)
                        print(f"从文本文件 {self.compile_options_file} 加载了编译选项")
            except Exception as e:
                print(f"加载编译选项文件 {self.compile_options_file} 失败: {e}", file=sys.stderr)

        print(f"使用总共 {len(options)} 个编译选项")
        return options

    def create_pch_file(self):
        """创建预编译头文件来处理外部库引用"""
        pch_content = """
        #ifndef REFLECTION_GENERATOR_PCH
        #define REFLECTION_GENERATOR_PCH

        // 标准库
        #include <string>
        #include <vector>
        #include <map>
        #include <unordered_map>
        #include <set>
        #include <unordered_set>
        #include <memory>
        #include <functional>
        #include <algorithm>
        #include <iostream>

        // 针对常见外部库的类型定义
        namespace ExternalTypeStubs {
            // 常见外部库的类型存根
            // 这些仅用于解析，不会参与实际编译
            template<typename T> class shared_ptr {};
            template<typename T> class unique_ptr {};
            template<typename... Args> class tuple {};
            template<typename T> class optional {};
            template<typename T> class expected {};
            template<typename T, typename E> class result {};
        }

        using namespace ExternalTypeStubs;

        #endif // REFLECTION_GENERATOR_PCH
        """

        fd, pch_file = tempfile.mkstemp(suffix='.hpp')
        os.write(fd, pch_content.encode('utf-8'))
        os.close(fd)
        return pch_file

    def process_header(self, header_file):
        """处理单个头文件"""
        # 加载编译选项
        options = self.load_compile_options()

        # 创建预编译头文件
        pch_file = self.create_pch_file()
        options.append(f'-include{pch_file}')

        print(f"正在解析头文件: {header_file}")

        try:
            # 使用不完全解析模式
            tu = self.index.parse(header_file, options,
                                  options=clang.cindex.TranslationUnit.PARSE_INCOMPLETE |
                                          clang.cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES |
                                          clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)

            # 记录诊断信息，但不立即退出
            has_errors = False
            for diag in tu.diagnostics:
                if diag.severity >= clang.cindex.Diagnostic.Error:
                    print(f"警告: {diag.spelling}", file=sys.stderr)
                    if diag.location.file:
                        print(f"位置: {diag.location.file}:{diag.location.line}:{diag.location.column}",
                              file=sys.stderr)
                    has_errors = True
                elif diag.severity == clang.cindex.Diagnostic.Warning:
                    # 只打印有用的警告信息
                    if 'could not find file' not in diag.spelling and 'not found' not in diag.spelling:
                        print(f"信息: {diag.spelling}")

            if has_errors:
                print(f"解析 {header_file} 时发现警告，将尝试提取可识别的类型...", file=sys.stderr)

            # 记录处理的文件
            self.processed_files.add(os.path.normpath(header_file))

            # 清空类型信息和访问状态
            self.type_infos = {}
            self.current_access_state = {}

            # 第一阶段：递归扫描所有类型，记录访问状态
            self.scan_and_record_access(tu.cursor)

            # 第二阶段：收集所有类型及其信息
            self.collect_all_types(tu.cursor)

            # 第三阶段：筛选出真正公开的类型
            self.filter_public_types()

            # 第四阶段：处理符合条件的类型
            self.process_filtered_types()

            return True
        except Exception as e:
            print(f"解析 {header_file} 异常: {e}", file=sys.stderr)
            return False
        finally:
            # 删除临时PCH文件
            try:
                os.unlink(pch_file)
            except:
                pass

    def scan_and_record_access(self, cursor):
        """扫描并记录所有类和结构体的访问状态"""
        # 生成唯一标识
        cursor_id = str(cursor.hash)

        if cursor.kind in [CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL]:
            # 设置默认访问状态
            if cursor.kind == CursorKind.CLASS_DECL:
                current_access = clang.cindex.AccessSpecifier.PRIVATE
            else:  # 结构体默认是public
                current_access = clang.cindex.AccessSpecifier.PUBLIC

            # 记录该类/结构体的当前访问状态
            self.current_access_state[cursor_id] = current_access

            # 处理类/结构体的子节点，记录访问修饰符和成员
            for child in cursor.get_children():
                if child.kind == CursorKind.CXX_ACCESS_SPEC_DECL:
                    # 更新当前访问状态
                    self.current_access_state[cursor_id] = child.access_specifier
                elif child.kind in [CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL, CursorKind.ENUM_DECL]:
                    # 记录成员类型的父级访问状态
                    child_id = str(child.hash)
                    self.current_access_state[child_id] = self.current_access_state.get(cursor_id,
                                                                                        clang.cindex.AccessSpecifier.PUBLIC)

                    # 递归处理嵌套类型
                    self.scan_and_record_access(child)

        # 递归处理其他节点
        for child in cursor.get_children():
            if child.kind not in [CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL]:
                self.scan_and_record_access(child)

    def is_external_type(self, cursor):
        """判断是否是外部库类型"""
        # 如果没有文件信息，可能是外部类型
        if not cursor.location.file:
            return True

        file_path = os.path.normpath(str(cursor.location.file.name))

        # 不在处理文件列表中的认为是外部类型
        if file_path not in self.processed_files:
            # 检查是否是系统头文件
            if file_path.startswith('/usr/') or 'include' in file_path:
                return True

        return False

    def is_static_member(self, cursor):
        """判断是否是静态成员"""
        # 对于字段，检查存储类别
        if cursor.kind == CursorKind.FIELD_DECL:
            # 检查存储类别是否为静态
            return cursor.storage_class == StorageClass.STATIC

        # 对于方法，检查是否是静态方法
        elif cursor.kind == CursorKind.CXX_METHOD:
            return cursor.is_static_method()

        return False

    def is_template_class(self, cursor):
        """判断是否是模板类(不包括模板特化)"""
        # 检查是否是类模板
        if cursor.kind == CursorKind.CLASS_TEMPLATE:
            return True

        # 检查类名是否包含模板参数但不是特化
        if '<' in cursor.spelling and '>' in cursor.spelling:
            # 检查是否有模板参数（非特化）
            for child in cursor.get_children():
                if child.kind == CursorKind.TEMPLATE_TYPE_PARAMETER or child.kind == CursorKind.TEMPLATE_NON_TYPE_PARAMETER:
                    return True

        return False

    def is_template_specialization(self, cursor):
        """判断是否是模板特化"""
        # 首先排除普通模板类
        if self.is_template_class(cursor):
            return False

        # 检查displayname中是否包含模板参数格式
        if '<' in cursor.displayname and '>' in cursor.displayname:
            # 如果是struct class还带有尖括号，很可能是模板特化
            return True

        return False

    def get_effective_access(self, cursor):
        """获取节点的有效访问修饰符，手动检查嵌套结构体是否在 private/protected 块中"""
        # 1. 如果不是嵌套类型（全局/命名空间级别），默认 public
        parent = cursor.semantic_parent
        if not parent or parent.kind not in (CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL):
            return clang.cindex.AccessSpecifier.PUBLIC

        # 2. 遍历父类的子节点，查找最近的访问修饰符
        current_access = (
            clang.cindex.AccessSpecifier.PRIVATE  # class 默认 private
            if parent.kind == CursorKind.CLASS_DECL
            else clang.cindex.AccessSpecifier.PUBLIC  # struct 默认 public
        )

        # 查找 cursor 是否在 private/protected 块中
        found_cursor = False
        for child in parent.get_children():
            if child.kind == CursorKind.CXX_ACCESS_SPEC_DECL:
                # 更新当前访问权限（遇到 private:/public:/protected:）
                current_access = child.access_specifier
            elif child == cursor:
                # 找到当前节点，返回最近的访问修饰符
                found_cursor = True
                break

        # 如果找到 cursor，返回最近的访问修饰符；否则返回默认值
        return current_access if found_cursor else (
            clang.cindex.AccessSpecifier.PRIVATE if parent.kind == CursorKind.CLASS_DECL
            else clang.cindex.AccessSpecifier.PUBLIC
        )

    def is_truly_public(self, cursor):
        """检查一个类型是否真正是公开的（考虑所有父级）"""
        current = cursor

        # 向上检查所有父级
        while current and current.kind != CursorKind.TRANSLATION_UNIT:
            # 对于类和结构体，检查访问修饰符
            if current.kind in [CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL, CursorKind.ENUM_DECL]:
                access = self.get_effective_access(current)
                if access != clang.cindex.AccessSpecifier.PUBLIC:
                    return False

            # 移动到父级
            current = current.semantic_parent

        return True

    def get_qualified_name(self, cursor):
        """获取光标的完全限定名称，包括所有父类和命名空间"""
        name_parts = []
        current = cursor

        # 获取当前类型的名称
        if self.is_template_specialization(cursor):
            name_parts.append(cursor.displayname)
        else:
            name_parts.append(cursor.spelling)

        # 向上遍历父级，收集所有的父类型和命名空间
        parent = cursor.semantic_parent
        while parent and parent.kind != CursorKind.TRANSLATION_UNIT:
            # 只处理命名的父级，忽略匿名命名空间和匿名类型
            if parent.spelling and 'unnamed' not in parent.spelling:
                name_parts.insert(0, parent.spelling)
            parent = parent.semantic_parent

        return "::".join(name_parts)

    def get_namespace_path(self, cursor):
        """获取光标所在的命名空间路径（不包括父类）"""
        namespaces = []
        parent = cursor.semantic_parent

        # 向上遍历父级，收集所有命名空间
        while parent and parent.kind == CursorKind.NAMESPACE:
            if parent.spelling:  # 只处理命名的命名空间，忽略匿名命名空间
                namespaces.insert(0, parent.spelling)
            parent = parent.semantic_parent

        return "::".join(namespaces) if namespaces else ""

    def collect_all_types(self, cursor):
        """递归收集所有类型信息"""
        # 递归遍历所有子节点
        for child in cursor.get_children():
            # 只处理当前文件中的定义
            if child.location.file:
                file_path = os.path.normpath(str(child.location.file.name))
                if file_path not in self.processed_files:
                    # 记录外部类型
                    if child.kind in [CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL, CursorKind.ENUM_DECL]:
                        self.external_types.add(child.spelling)
                    # 不再往下处理外部文件
                    continue

            # 处理类型定义
            if child.kind in [CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL, CursorKind.ENUM_DECL]:
                # 获取类型名称
                type_name = child.displayname if self.is_template_specialization(child) else child.spelling
                if not type_name or 'unnamed' in type_name:
                    # 跳过未命名类型
                    continue

                # 跳过普通模板类
                if self.is_template_class(child) and not self.is_template_specialization(child):
                    self.skipped_types.append(f"{type_name} (普通模板类)")
                    print(f"跳过普通模板类: {type_name}")
                    continue

                # 跳过外部类型
                if self.is_external_type(child):
                    self.external_types.add(type_name)
                    continue

                # 获取父节点，确定访问权限上下文
                parent = child.semantic_parent
                parent_is_class = parent.kind == CursorKind.CLASS_DECL if parent else False
                parent_is_struct = parent.kind == CursorKind.STRUCT_DECL if parent else False

                # 获取有效的访问修饰符
                access = self.get_effective_access(child)

                # 检查是否真正是公开的
                is_public = access == clang.cindex.AccessSpecifier.PUBLIC

                # 获取完全限定名
                qualified_name = self.get_qualified_name(child)

                # 存储类型信息
                self.type_infos[qualified_name] = {
                    'cursor': child,
                    'name': type_name,
                    'qualified_name': qualified_name,
                    'parent': parent,
                    'parent_is_class': parent_is_class,
                    'parent_is_struct': parent_is_struct,
                    'access': access,
                    'is_public': is_public,
                    'is_truly_public': self.is_truly_public(child),
                    'is_template': self.is_template_class(child),
                    'is_specialization': self.is_template_specialization(child),
                    'namespace': self.get_namespace_path(child)
                }

                # 调试信息
                access_str = "PUBLIC" if is_public else "PRIVATE/PROTECTED"
                parent_str = f"{parent.spelling}({parent.kind})" if parent else "全局"
                print(f"收集类型: {qualified_name}, 访问权限: {access_str}, 父级: {parent_str}")

            # 继续递归处理子节点
            self.collect_all_types(child)

    def filter_public_types(self):
        """过滤出真正公开的类型"""
        for name, info in list(self.type_infos.items()):
            # 检查是否真正公开（考虑所有父级）
            if not info['is_truly_public']:
                self.skipped_types.append(f"{info['name']} (非公开类型)")
                print(f"跳过非公开类型: {info['name']} (原因: 当前或父级访问权限非公开)")
                self.type_infos.pop(name)
                continue

    def process_filtered_types(self):
        """处理过滤后的类型"""
        for name, info in self.type_infos.items():
            cursor = info['cursor']
            if cursor.kind in [CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL]:
                self.process_class(cursor)
            elif cursor.kind == CursorKind.ENUM_DECL:
                self.process_enum(cursor)

    def get_fully_qualified_type(self, type_obj):
        """获取完整的类型名称（包括模板参数），处理 ELABORATED 类型"""
        # 1. 如果是 ELABORATED 类型（如 struct Ref<Device>），尝试获取底层类型
        if type_obj.kind == TypeKind.ELABORATED:
            # 获取被修饰的底层类型（如 Ref<Device>）
            canonical_type = type_obj.get_canonical()
            if canonical_type.kind != TypeKind.ELABORATED:
                return self.get_fully_qualified_type(canonical_type)

            # 如果仍然是 ELABORATED，尝试从声明获取名称
            decl = type_obj.get_declaration()
            if decl:
                return self.get_qualified_name(decl)  # 返回完全限定名（如 "Luma::Ref<Device>"）

        # 2. 如果是模板实例化（如 Ref<Device>），获取模板名 + 参数
        if type_obj.kind == TypeKind.UNEXPOSED:
            decl = type_obj.get_declaration()
            if decl and decl.kind == CursorKind.CLASS_TEMPLATE:
                template_name = decl.spelling
                template_args = []
                for child in decl.get_children():
                    if child.kind == CursorKind.TEMPLATE_TYPE_PARAMETER:
                        template_args.append(child.spelling)
                return f"{template_name}<{', '.join(template_args)}>"

        # 3. 默认返回类型的拼写（可能不完整，但作为后备方案）
        return type_obj.spelling

    def process_class(self, cursor):
        """处理类或结构体"""
        # 获取类名，对于模板特化使用displayname
        if self.is_template_specialization(cursor):
            class_name = cursor.displayname
        else:
            class_name = cursor.spelling

        # 跳过未命名类
        if not class_name or 'unnamed' in class_name:
            return

        # 跳过模板类（但不跳过模板特化）
        if cursor.kind == CursorKind.CLASS_TEMPLATE or (
                self.is_template_class(cursor) and not self.is_template_specialization(cursor)):
            # self.skipped_types.append(f"{class_name} (普通模板类)") # 此信息已在collect_all_types中记录
            return

        # 跳过外部类型 (此检查可能多余，因为collect_all_types已过滤)
        if self.is_external_type(cursor):
            # self.external_types.add(class_name) # 此信息已在collect_all_types中记录
            return

        # 获取完全限定名
        fully_qualified_name = self.get_qualified_name(cursor)

        # 保存简单类名（用于后续替换自引用类型）
        # simple_class_name 用于 correct_type_name 中的自引用检查
        simple_class_name_for_self_ref = class_name.split('<')[0] if '<' in class_name else class_name

        # 获取命名空间（用于组织输出）
        namespace = self.get_namespace_path(cursor)

        # 检查该类型是否已经注册过，避免重复
        if fully_qualified_name in self.registered_types:
            # print(f"跳过已注册的类型: {fully_qualified_name}") # 此信息已在process_filtered_types中处理
            return

        # 记录类型已被注册 (实际应在成功生成代码后添加，此处提前仅为逻辑占位)
        # self.registered_types.add(fully_qualified_name) # 移至实际生成代码后

        properties = []
        methods = []
        constructors = []

        # --- 修改开始: 为 correct_type_name 构建更全面的类型查找表 ---
        # local_namespace_types 将包含从 self.type_infos (所有收集到的类型) 中提取的 simple_name -> FQN 映射
        local_namespace_types = {}
        for fqn_collected_type in self.type_infos.keys():  # self.type_infos 的键是收集到的类型的FQN
            simple_name_collected = fqn_collected_type.split("::")[-1]

            # 对于模板类型如 "MyClass<int>"，我们希望简单名称是 "MyClass" 作为键
            if '<' in simple_name_collected:
                simple_name_collected = simple_name_collected.split("<")[0]

            # 如果简单名称发生冲突（例如不同命名空间下的同名类型），
            # 这里的基本策略是先收集到的优先。在更复杂的场景下可能需要更精细的策略。
            if simple_name_collected not in local_namespace_types:
                local_namespace_types[simple_name_collected] = fqn_collected_type
        # --- 修改结束 ---

        # 收集类中的成员和方法
        for member in cursor.get_children():
            access = self.get_effective_access(member)
            if access == clang.cindex.AccessSpecifier.PUBLIC and not self.is_static_member(member):
                if member.kind == CursorKind.FIELD_DECL:
                    field_type_from_clang = self.get_fully_qualified_type(member.type)  # 使用您脚本中已有的辅助函数
                    is_external = any(ext_type in field_type_from_clang for ext_type in self.external_types)
                    if not is_external:
                        properties.append(member.spelling)
                elif member.kind == CursorKind.CXX_METHOD:
                    if member.spelling != class_name and not member.spelling.startswith('~'):
                        return_type_str = member.result_type.spelling  # 原始字符串
                        param_types_str_list = [param.type.spelling for param in member.get_arguments()]  # 原始字符串列表
                        is_const_method = member.is_const_method()

                        # 检查原始类型字符串中是否有外部类型 (简单检查)
                        is_external = False
                        if any(ext_type in return_type_str for ext_type in self.external_types):
                            is_external = True
                        if not is_external:
                            for pt_str in param_types_str_list:
                                if any(ext_type in pt_str for ext_type in self.external_types):
                                    is_external = True
                                    break

                        if not is_external:
                            methods.append({
                                'name': member.spelling,
                                'return_type_orig': return_type_str,  # 存储原始，待修正
                                'param_types_orig': param_types_str_list,  # 存储原始，待修正
                                'is_const': is_const_method
                            })
                elif member.kind == CursorKind.CONSTRUCTOR:
                    param_types_str_list = []
                    is_external = False
                    for param in member.get_arguments():
                        type_str = param.type.spelling  # 原始字符串
                        if any(ext_type in type_str for ext_type in self.external_types):
                            is_external = True
                            break
                        param_types_str_list.append(type_str)

                    if not is_external:
                        constructors.append(param_types_str_list)  # 存储原始，待修正

        # 如果没有可反射的成员，则不继续生成该类的注册代码
        if not properties and not methods and not constructors:
            self.skipped_types.append(f"{fully_qualified_name} (无公开可反射成员)")
            return  # 不将此类加入 registered_types 或 resolved_types

        # 实际将要生成代码，此时才记录
        self.registered_types.add(fully_qualified_name)
        self.resolved_types.add(fully_qualified_name)

        # 准备生成注册代码
        short_name_for_comment = class_name.split('<')[0] if '<' in class_name else class_name
        registration = f"    // {short_name_for_comment} ({fully_qualified_name}) 类的反射注册\n"
        registration += f"    Registry_<{fully_qualified_name}>()\n"

        for prop in properties:
            registration += f"        .property(\"{prop}\", &{fully_qualified_name}::{prop})\n"

        # ======================================================================
        # 完整 的 correct_type_name 嵌套函数定义 (使用 local_namespace_types)
        # ======================================================================
        def correct_type_name(type_name_str_to_correct):
            """
            修正类型名称字符串，尝试将其简单名称部分替换为已知的FQN。
            处理 const, 指针(*), 引用(&)。
            """
            original_full_str = type_name_str_to_correct  # 例如 "const MyType&", "MyType *", "MyType"

            if not type_name_str_to_correct:
                return ""

            # 提取前缀 (const), 后缀 (*, &), 和主类型名
            prefix = ""
            suffix = ""
            main_part = type_name_str_to_correct

            if main_part.startswith("const "):
                prefix = "const "
                main_part = main_part[len("const "):].strip()

            # 处理后缀，注意可能有多个指针，但通常只有一个引用
            # 这个简化处理只取最后一个*或&
            if main_part.endswith("&&"):  # 右值引用优先
                suffix = "&&"
                main_part = main_part[:-2].strip()
            elif main_part.endswith("&"):  # 左值引用
                suffix = "&"
                main_part = main_part[:-1].strip()
            elif main_part.endswith("*"):  # 指针
                suffix = "*"
                main_part = main_part[:-1].strip()

            # main_part 现在应该是核心类型名，如 "CommandPool", "int", "std::vector<int>"

            # 如果核心类型名已包含 "::"，则认为它已经是限定名或来自std等已知命名空间
            if "::" in main_part:
                return original_full_str  # 直接返回原始字符串，因为假定它已ok或来自外部(如std::)

            # 检查是否是自引用 (当前类的简单名称)
            # simple_class_name_for_self_ref 来自外部 process_class 方法作用域
            if main_part == simple_class_name_for_self_ref:
                return prefix + fully_qualified_name + suffix  # fully_qualified_name 是当前类的FQN

            # 检查是否与 local_namespace_types 中的已知类型匹配
            # local_namespace_types 来自外部 process_class 方法作用域
            if main_part in local_namespace_types:
                return prefix + local_namespace_types[main_part] + suffix

            # 如果没有找到匹配，并且它不是基本类型 (这里简单判断，更复杂的可能需要类型检查)
            # 则返回原始的、可能未完全限定的名称。基本类型如 "int" 会落到这里。
            return original_full_str

        # ======================================================================
        # correct_type_name 定义结束
        # ======================================================================

        for method_info in methods:
            method_name = method_info['name']
            # 使用 correct_type_name 修正原始类型字符串
            corrected_return_type = correct_type_name(method_info['return_type_orig'])
            corrected_param_types = [correct_type_name(pt) for pt in method_info['param_types_orig']]
            is_const = method_info['is_const']

            const_suffix = " const" if is_const else ""

            param_list_for_signature = ', '.join(corrected_param_types)
            function_ptr_cast = f"({corrected_return_type} ({fully_qualified_name}::*)({param_list_for_signature}){const_suffix})"

            template_args_list = [corrected_return_type] + corrected_param_types
            template_args_str = ", ".join(template_args_list)

            registration += f"        .method<{template_args_str}>(\"{method_name}\", {function_ptr_cast}&{fully_qualified_name}::{method_name})\n"

        for ctor_params_orig_list in constructors:
            if not ctor_params_orig_list:
                registration += f"        .constructor<>()\n"
            else:
                # 使用 correct_type_name 修正原始类型字符串
                corrected_ctor_params = [correct_type_name(pt) for pt in ctor_params_orig_list]
                registration += f"        .constructor<{', '.join(corrected_ctor_params)}>()\n"

        registration += "    ;"
        self.namespace_registrations[namespace].append(registration)

    def process_enum(self, cursor):
        """处理枚举类型"""
        enum_name = cursor.spelling

        # 跳过未命名枚举
        if not enum_name or 'unnamed' in enum_name:
            return

        # 跳过外部类型
        if self.is_external_type(cursor):
            self.external_types.add(enum_name)
            return

        # 获取完全限定名
        fully_qualified_name = self.get_qualified_name(cursor)

        # 获取命名空间（用于组织输出）
        namespace = self.get_namespace_path(cursor)

        # 检查该枚举是否已经注册过，避免重复
        if fully_qualified_name in self.registered_types:
            print(f"跳过已注册的枚举: {fully_qualified_name}")
            return

        # 记录枚举已被注册
        self.registered_types.add(fully_qualified_name)

        # 检查是否是枚举类 (enum class)
        is_enum_class = cursor.is_scoped_enum()

        # 检查枚举的基础类型
        enum_type = "int"  # 默认基础类型
        try:
            # 尝试获取枚举的类型
            enum_type_spelling = cursor.enum_type.spelling
            if enum_type_spelling:
                enum_type = enum_type_spelling
        except:
            # 如果无法获取类型，使用默认的int
            pass

        enum_values = []

        # 收集枚举值
        for enum_value in cursor.get_children():
            if enum_value.kind == CursorKind.ENUM_CONSTANT_DECL:
                value_name = enum_value.spelling
                enum_values.append(value_name)

        # 生成枚举注册代码
        if enum_values:
            self.resolved_types.add(fully_qualified_name)

            # 生成枚举注册代码（不包含RTTM_REGISTRATION块）
            registration = f"    // {enum_name} 枚举的反射注册 (基础类型: {enum_type})\n"
            registration += f"    Enum_<{fully_qualified_name}>()\n"

            for value in enum_values:
                if is_enum_class:
                    registration += f"        .value(\"{value}\", {fully_qualified_name}::{value})\n"
                else:
                    # 对于普通枚举，使用完全限定名访问枚举值
                    registration += f"        .value(\"{value}\", {fully_qualified_name}::{value})\n"

            registration += "    ;"
            self.namespace_registrations[namespace].append(registration)

    def generate(self):
        """生成反射代码文件"""
        try:
            with open(self.output_file, 'w') as f:
                # 写入包含文件
                f.write("#include <RTTM/RTTM.hpp>\n")  # 使用尖括号包含

                # 添加所有处理过的头文件
                for header in sorted(self.processed_files):
                    f.write(f"#include \"{header}\"\n")
                f.write("\n")

                f.write("// 自动生成的反射注册代码，请勿修改\n")
                f.write("// 此代码仅包含头文件中直接定义的类型，不包含外部库引用\n")
                f.write("// 注意：私有成员、保护成员、静态成员变量和静态方法已被排除\n")
                f.write("// 普通模板类被排除，但模板特化会被包含\n")
                f.write("// 所有非公开的嵌套类型都被排除，包括嵌套在非公开类型中的公开类型\n")
                f.write("using namespace RTTM;\n\n")

                # 添加外部类型注释
                if self.external_types:
                    f.write("// 以下外部类型被排除在反射之外:\n")
                    for ext_type in sorted(self.external_types):
                        f.write(f"// - {ext_type}\n")
                    f.write("\n")

                # 添加跳过类型的注释
                if self.skipped_types:
                    f.write("// 以下类型在处理过程中被跳过:\n")
                    for skipped in self.skipped_types:
                        f.write(f"// - {skipped}\n")
                    f.write("\n")

                # 按命名空间组织的所有类型注册代码
                all_registrations = {}
                for namespace, registrations in self.namespace_registrations.items():
                    all_registrations[namespace] = registrations

                # 检查是否有需要注册的类型
                has_types_to_register = any(registrations for registrations in all_registrations.values())

                if has_types_to_register:
                    # 全局注册块
                    f.write("RTTM_REGISTRATION {\n")

                    # 首先处理全局命名空间的注册
                    if "" in all_registrations and all_registrations[""]:
                        for code in all_registrations[""]:
                            f.write(f"{code}\n\n")

                    # 然后按命名空间组织写入其他注册代码
                    for namespace, registrations in sorted(all_registrations.items()):
                        if namespace == "":  # 已经处理过全局命名空间
                            continue

                        if registrations:
                            f.write(f"    // {namespace} 命名空间中的类型\n")
                            for code in registrations:
                                f.write(f"{code}\n\n")

                    f.write("}\n")
                else:
                    f.write("// 未找到需要反射的类型\n")

                print(f"成功生成反射代码: {self.output_file}")
                return True
        except Exception as e:
            print(f"生成反射代码失败: {e}", file=sys.stderr)
            return False


def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(description='生成C++类的反射代码')

    # 添加一组互斥的参数
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--headers-list', help='包含头文件列表的文本文件')
    group.add_argument('input_file', nargs='?', help='输入头文件')

    parser.add_argument('--output', dest='output_file', required=True, help='输出文件路径')
    parser.add_argument('--options', dest='compile_options_file', help='编译选项文件路径')
    parser.add_argument('--include-paths', dest='include_paths', help='包含路径，以逗号分隔')

    # 兼容旧的位置参数格式
    parser.add_argument('old_output_file', nargs='?', help='旧格式的输出文件参数')
    parser.add_argument('old_compile_options_file', nargs='?', help='旧格式的编译选项文件参数')
    parser.add_argument('old_include_paths', nargs='?', help='旧格式的包含路径参数')

    return parser.parse_args()


def main():
    """主函数"""
    args = parse_args()

    # 处理包含路径
    include_paths = []
    if args.include_paths:
        include_paths = args.include_paths.split(',')
    elif args.old_include_paths:
        include_paths = args.old_include_paths.split(',')

    # 创建反射生成器
    generator = ReflectionGenerator(
        args.output_file,
        args.compile_options_file or args.old_compile_options_file,
        include_paths
    )

    # 处理头文件
    if args.headers_list:
        # 从头文件列表文件中读取头文件
        headers = []
        try:
            with open(args.headers_list, 'r') as f:
                headers = [line.strip() for line in f if line.strip()]
        except Exception as e:
            print(f"读取头文件列表失败: {e}", file=sys.stderr)
            return 2

        print(f"从 {args.headers_list} 读取了 {len(headers)} 个头文件")

        success = True
        for header in headers:
            if not generator.process_header(header):
                success = False
                print(f"警告: 处理 {header} 时出现错误", file=sys.stderr)
    else:
        # 处理单个头文件
        success = generator.process_header(args.input_file)

    # 生成反射代码
    if generator.generate():
        return 0 if success else 1
    else:
        return 2


if __name__ == "__main__":
    sys.exit(main())