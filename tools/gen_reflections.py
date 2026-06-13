#!/bin/python3
# Script to generate reflection bindings for classes and functions
# in the LCore library.
# This script scans the header files for classes and functions
# and generates reflection metadata for [[reflect]] annotated items.

# Command: this.py <input_header...> / <input_folder> -o <output_file> -I <include_path...>
# Example: this.py include/lcore/ -o include/lcore/reflection/generated.hpp -I include/ -I /usr/include/ ...

try:
    import clang.cindex as cindex
except ImportError:
    print("Error: clang.cindex module not found. Please install clang bindings for Python.")
    exit(1)
    
import argparse

class Parser:
    def __init__(self, include_paths: list[str]) -> None:
        self.index = cindex.Index.create()
        self.include_paths = include_paths
        self.classes = []
        self.functions = []
        self.enums = []
        
    def parse(self, input: str):
        '''
        Parse the input content, and extract classes, functions, and enums.
        '''
        tu = self.index.parse(input, args=['-x', 'c++', '-std=c++20'] + [f'-I{path}' for path in self.include_paths])
        
        def _handle_tree(node: cindex.Cursor):
            for walk in node.walk_preorder():
                # if walk.kind == cindex.CursorKind.CLASS_DECL and walk.is_definition():
                #     self.classes.append(walk)
                # elif walk.kind == cindex.CursorKind.FUNCTION_DECL and walk.is_definition():
                #     self.functions.append(walk)
                # elif walk.kind == cindex.CursorKind.ENUM_DECL and walk.is_definition():
                #     self.enums.append(walk)
                print(walk.kind, walk.spelling, walk.kind.is_attribute())
                
        _handle_tree(tu.cursor)
        
    def generate(self, output: str):
        '''
        Generate reflection metadata and write to output file.
        '''
        with open(output, 'w') as f:
            f.write('// Generated reflection metadata\n')
            f.write('#pragma once\n\n')
            f.write('#include "lcore/reflecction/class.hpp"\n')
            f.write('#include "lcore/reflecction/function.hpp"\n')
            f.write('#include "lcore/reflecction/enum.hpp"\n\n')
            
            f.write('using namespace LCORE_NAMESPACE;\n')
            f.write('using namespace LCORE_REFLECTION_NAMESPACE_NAME;\n\n')
            
            for cls in self.classes:
                namespace_name = '::'.join([n.spelling for n in cls.get_namespace()])
                serialized_namespace_name = namespace_name.replace('::', '_') if namespace_name else ''
                class_name = cls.spelling
                serialized_name = f'{serialized_namespace_name}_{class_name}' if serialized_namespace_name else class_name
                f.write(f'// Reflection for class {namespace_name}::{class_name}\n')
                f.write(f'class {serialized_name}_Reflection : public ClassReflection {{\n')
                f.write('public:\n')
                f.write(f'    {serialized_name}_Reflection() : ClassReflection(\n')
                f.write(f'        TypeInfo::Get<{namespace_name}::{class_name}>(),\n')
                f.write(f'        sizeof({namespace_name}::{class_name}),\n')
                f.write(f'        alignof({namespace_name}::{class_name}),\n')
                f.write('        {\n')
                # Fields
                fields = [c for c in cls.get_children() if c.kind == cindex.CursorKind.FIELD_DECL]
                for field in fields:
                    f.write(f'            ReflectedField(TypeInfo::Get<{field.type.spelling}>(), "{field.spelling}", offsetof({namespace_name}::{class_name}, {field.spelling})),\n')
                f.write('        },\n')
                f.write('        {\n')
                # Methods
                methods = [c for c in cls.get_children() if c.kind == cindex.CursorKind.CXX_METHOD]
                for method in methods:
                    ret_type = method.result_type.spelling
                    params = ', '.join([param.type.spelling for param in method.get_arguments()])
                    f.write(f'            ReflectedMethod(TypeInfo::Get<{ret_type}({params})>(), "{method.spelling}", &{namespace_name}::{class_name}::{method.spelling}),\n')
                f.write('        },\n')
                f.write('        {\n')
                # Constructors
                constructors = [c for c in cls.get_children() if c.kind == cindex.CursorKind.CONSTRUCTOR]
                for ctor in constructors:
                    params = ', '.join([param.type.spelling for param in ctor.get_arguments()])
                    f.write(f'            ReflectedConstructor(TypeInfo::Get<void({params})>(), &{namespace_name}::{class_name}::{ctor.spelling}),\n')
                f.write('        },\n')
                f.write('        {\n')
                # Static Fields
                static_fields = [c for c in cls.get_children() if c.kind == cindex.CursorKind.VAR_DECL and c.is_static_member()]
                for sfield in static_fields:
                    f.write(f'            ReflectedStaticField(TypeInfo::Get<{sfield.type.spelling}>(), "{sfield.spelling}", &{namespace_name}::{class_name}::{sfield.spelling}),\n')
                f.write('        },\n')
                f.write('        {\n')
                # Static Methods
                static_methods = [c for c in cls.get_children() if c.kind == cindex.CursorKind.CXX_METHOD and c.is_static_method()]
                for smethod in static_methods:
                    ret_type = smethod.result_type.spelling
                    params = ', '.join([param.type.spelling for param in smethod.get_arguments()])
                    f.write(f'            ReflectedStaticMethod(TypeInfo::Get<{ret_type}({params})>(), "{smethod.spelling}", &{namespace_name}::{class_name}::{smethod.spelling}),\n')
                f.write('        },\n')
                # f.write('        {\n')
                # # Static Operators
                # static_operators = [c for c in cls.get_children() if c.kind == cindex.CursorKind.CXX_METHOD and c.is_static_method() and c.is_operator()]
                # for soperator in static_operators:
                #     ret_type = soperator.result_type.spelling
                #     params = ', '.join([param.type.spelling for param in soperator.get_arguments()])
                #     f.write(f'            ReflectedStaticOperator(TypeInfo::Get<{ret_type}({params})>(), "{soperator.spelling}", &{namespace_name}::{class_name}::{soperator.spelling}),\n')
                # f.write('        }\n')
                f.write('    ) {}\n')
                f.write('};\n\n')

if __name__ == '__main__':
    args = argparse.ArgumentParser(description="Generate reflection metadata for LCore library.")
    args.add_argument('input', nargs='+', help="Input header files or folders to scan.")
    args.add_argument('-o', '--output', required=True, help="Output file for generated reflection metadata.")
    # args.add_argument('-l', '--language', help="Language file for parsing rules. --- IGNORE ---")
    args.add_argument('-I', '--include', action='append', help="Additional include paths.")
    parsed_args = args.parse_args()
    
    include_paths = parsed_args.include if parsed_args.include else []
    parser = Parser(include_paths)
    for input in parsed_args.input:
        parser.parse(input)
    parser.generate(parsed_args.output)
    