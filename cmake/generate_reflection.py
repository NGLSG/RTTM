#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 使用clang库解析C++代码并生成反射代码

import sys
import os
import re
import clang.cindex
from clang.cindex import CursorKind, TypeKind
import json
import tempfile


class ReflectionGenerator:
    def __init__(self, input_file, output_file, compile_options_file=None, include_paths=None):
        """初始化反射代码生成器"""
        self.input_file = input_file
        self.output_file = output_file
        self.compile_options_file = compile_options_file
        self.include_paths = include_paths or []
        self.index = clang.cindex.Index.create()
        self.registrations = []

        # 记录已处理的源文件
        self.processed_files = set()

        # 记录无法解析的外部类型
        self.external_types = set()

        # 记录成功解析的类型
        self.resolved_types = set()

        # 当前命名空间路径
        self.current_namespace = []

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

        # 仅识别当前头文件中定义的类
        options.append('--analyze-headers-only')

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

    def parse(self):
        """解析输入文件"""
        # 加载编译选项
        options = self.load_compile_options()

        # 创建预编译头文件
        pch_file = self.create_pch_file()
        options.append(f'-include{pch_file}')

        print(f"正在解析头文件: {self.input_file}")
        print(f"使用以下包含路径:")
        for path in self.include_paths:
            print(f"  {path}")

        try:
            # 使用不完全解析模式
            tu = self.index.parse(self.input_file, options,
                                  options=clang.cindex.TranslationUnit.PARSE_INCOMPLETE |
                                          clang.cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES |
                                          clang.cindex.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)

            # 记录诊断信息，但不立即退出
            has_errors = False
            for diag in tu.diagnostics:
                if diag.severity >= clang.cindex.Diagnostic.Error:
                    print(f"警告: {diag.spelling}", file=sys.stderr)
                    if diag.location.file:
                        print(f"位置: {diag.location.file}:{diag.location.line}:{diag.location.column}", file=sys.stderr)
                    has_errors = True
                elif diag.severity == clang.cindex.Diagnostic.Warning:
                    # 只打印有用的警告信息
                    if 'could not find file' not in diag.spelling and 'not found' not in diag.spelling:
                        print(f"信息: {diag.spelling}")

            if has_errors:
                print("发现解析警告，将尝试提取可识别的类型...", file=sys.stderr)

            # 只处理当前文件
            self.processed_files.add(os.path.normpath(self.input_file))
            self.traverse(tu.cursor)

            # 删除临时PCH文件
            try:
                os.unlink(pch_file)
            except:
                pass

            # 即使有解析警告也继续生成
            print(f"解析完成。成功识别 {len(self.resolved_types)} 个类型，跳过 {len(self.external_types)} 个外部类型。")
            return True
        except Exception as e:
            print(f"解析异常: {e}", file=sys.stderr)
            # 删除临时PCH文件
            try:
                os.unlink(pch_file)
            except:
                pass
            return False

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

    def get_qualified_name(self, name):
        """获取带命名空间的完整类型名称"""
        if not self.current_namespace:
            return name

        return "::".join(self.current_namespace) + "::" + name

    def traverse(self, cursor):
        """遍历AST语法树"""
        # 处理命名空间
        if cursor.kind == CursorKind.NAMESPACE:
            # 进入新的命名空间
            self.current_namespace.append(cursor.spelling)

            # 递归处理命名空间内的内容
            for child in cursor.get_children():
                self.traverse(child)

            # 离开命名空间
            self.current_namespace.pop()
            return

        for child in cursor.get_children():
            # 获取文件位置
            if child.location.file:
                file_path = os.path.normpath(str(child.location.file.name))

                # 只处理当前文件中的定义
                if file_path not in self.processed_files:
                    # 记录外部类型但不处理
                    if child.kind == CursorKind.CLASS_DECL or child.kind == CursorKind.STRUCT_DECL or child.kind == CursorKind.ENUM_DECL:
                        if child.spelling:
                            full_name = self.get_qualified_name(child.spelling)
                            self.external_types.add(full_name)
                    continue

            # 处理命名空间
            if child.kind == CursorKind.NAMESPACE:
                self.traverse(child)
            # 处理类/结构体
            elif child.kind == CursorKind.CLASS_DECL or child.kind == CursorKind.STRUCT_DECL:
                if not self.is_external_type(child):
                    self.process_class(child)
            # 处理枚举
            elif child.kind == CursorKind.ENUM_DECL:
                if not self.is_external_type(child):
                    self.process_enum(child)
            else:
                # 递归处理其他节点
                self.traverse(child)

    def process_class(self, cursor):
        """处理类或结构体"""
        class_name = cursor.spelling
        if not class_name or cursor.access_specifier == clang.cindex.AccessSpecifier.PRIVATE:
            return

        # 跳过未命名类
        if 'unnamed' in class_name:
            return

        # 跳过模板实例化
        if cursor.kind == CursorKind.CLASS_TEMPLATE:
            return

        # 获取带命名空间的完整类名
        full_class_name = self.get_qualified_name(class_name)

        # 跳过外部类型
        if self.is_external_type(cursor):
            self.external_types.add(full_class_name)
            return

        properties = []
        methods = []
        constructors = []

        # 收集类中的成员和方法
        for member in cursor.get_children():
            # 只考虑公共成员
            if member.access_specifier == clang.cindex.AccessSpecifier.PUBLIC:
                if member.kind == CursorKind.FIELD_DECL:
                    # 检查字段类型是否是外部类型
                    field_type = member.type.spelling
                    is_external = False
                    for ext_type in self.external_types:
                        if ext_type in field_type:
                            is_external = True
                            break

                    if not is_external:
                        properties.append(member.spelling)
                elif member.kind == CursorKind.CXX_METHOD:
                    if member.spelling != class_name and not member.spelling.startswith('~'):
                        # 检查方法返回类型和参数类型
                        return_type = member.result_type.spelling
                        param_types = [param.type.spelling for param in member.get_arguments()]

                        is_external = False
                        for ext_type in self.external_types:
                            if ext_type in return_type:
                                is_external = True
                                break
                            for param_type in param_types:
                                if ext_type in param_type:
                                    is_external = True
                                    break
                            if is_external:
                                break

                        if not is_external:
                            methods.append(member.spelling)
                elif member.kind == CursorKind.CONSTRUCTOR:
                    # 分析构造函数参数类型
                    param_types = []
                    is_external = False

                    for param in member.get_arguments():
                        type_str = param.type.spelling

                        for ext_type in self.external_types:
                            if ext_type in type_str:
                                is_external = True
                                break

                        if not is_external:
                            param_types.append(type_str)

                    if not is_external:
                        constructors.append(param_types)

        # 生成注册代码
        if properties or methods or constructors:
            self.resolved_types.add(full_class_name)
            registration = f"RTTM_REGISTRATION {{\n"
            registration += f"    Registry_<{full_class_name}>()\n"

            # 添加属性
            for prop in properties:
                registration += f"        .property(\"{prop}\", &{full_class_name}::{prop})\n"

            # 添加方法
            for method in methods:
                registration += f"        .method(\"{method}\", &{full_class_name}::{method})\n"

            # 添加构造函数
            for ctor_params in constructors:
                if not ctor_params:
                    registration += f"        .constructor<>()\n"
                else:
                    registration += f"        .constructor<{', '.join(ctor_params)}>()\n"

            registration += ";\n}"
            self.registrations.append(registration)

    def process_enum(self, cursor):
        """处理枚举类型"""
        enum_name = cursor.spelling

        # 跳过未命名枚举
        if not enum_name or 'unnamed' in enum_name:
            return

        # 获取带命名空间的完整枚举名
        full_enum_name = self.get_qualified_name(enum_name)

        # 跳过外部类型
        if self.is_external_type(cursor):
            self.external_types.add(full_enum_name)
            return

        # 检查是否是枚举类
        is_enum_class = cursor.is_scoped_enum()

        enum_values = []

        # 收集枚举值
        for enum_value in cursor.get_children():
            if enum_value.kind == CursorKind.ENUM_CONSTANT_DECL:
                value_name = enum_value.spelling
                enum_values.append(value_name)

        # 生成枚举注册代码
        if enum_values:
            self.resolved_types.add(full_enum_name)
            registration = f"RTTM_REGISTRATION {{\n"

            # 枚举注册
            registration += f"    Enum_<{full_enum_name}>()\n"

            for value in enum_values:
                if is_enum_class:
                    registration += f"        .value(\"{value}\", {full_enum_name}::{value})\n"
                else:
                    # 对于非枚举类，枚举值可能在全局作用域或命名空间内
                    if self.current_namespace:
                        value_with_ns = "::".join(self.current_namespace) + "::" + value
                        registration += f"        .value(\"{value}\", {value_with_ns})\n"
                    else:
                        registration += f"        .value(\"{value}\", {value})\n"

            registration += ";\n}"
            self.registrations.append(registration)

    def generate(self):
        """生成反射代码文件"""
        try:
            with open(self.output_file, 'w') as f:
                # 写入包含文件
                f.write("#include <RTTM/RTTM.hpp>\n")  # 使用尖括号包含
                f.write("#include \"" + self.input_file + "\"\n\n")

                f.write("// 自动生成的反射注册代码，请勿修改\n")
                f.write("// 此代码仅包含头文件中直接定义的类型，不包含外部库引用\n")
                f.write("using namespace RTTM;\n\n")

                # 添加外部类型注释
                if self.external_types:
                    f.write("// 以下外部类型被排除在反射之外:\n")
                    for ext_type in sorted(self.external_types):
                        f.write(f"// - {ext_type}\n")
                    f.write("\n")

                # 写入注册代码
                if self.registrations:
                    for code in self.registrations:
                        f.write(f"{code}\n\n")
                else:
                    f.write("// 未找到需要反射的类型\n")

                print(f"成功生成反射代码: {self.output_file}")
                return True
        except Exception as e:
            print(f"生成反射代码失败: {e}", file=sys.stderr)
            return False


def main():
    """主函数"""
    if len(sys.argv) < 3:
        print("用法: python generate_reflection.py <输入头文件> <输出文件> [编译选项文件] [包含路径1,包含路径2,...]", file=sys.stderr)
        return 1

    input_file = sys.argv[1]
    output_file = sys.argv[2]
    compile_options_file = sys.argv[3] if len(sys.argv) > 3 else None

    # 解析包含路径参数
    include_paths = []
    if len(sys.argv) > 4:
        include_paths = sys.argv[4].split(',')

    generator = ReflectionGenerator(input_file, output_file, compile_options_file, include_paths)
    if generator.parse():
        generator.generate()
        print(f"成功为 {input_file} 生成反射代码: {output_file}")
        return 0
    else:
        print(f"警告: 解析 {input_file} 时出现错误，尝试生成可能不完整的反射代码")
        if generator.generate():
            return 1  # 生成不完整的代码，返回非零值表示有警告
        else:
            return 2  # 生成失败，返回更高的错误码


if __name__ == "__main__":
    sys.exit(main())