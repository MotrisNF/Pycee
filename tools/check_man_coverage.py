#!/usr/bin/env python3
"""
Checks that the manual has a page for everything a program can name:
every keyword (src/front/lexer.c), every global native and every member
of a module (include/resolver/natives.h), and every method of every type
(the method tables in src/types/ and src/runtime/methods.c), as
type.method. A topic is covered by a page whose first line lists it.

    python3 tools/check_man_coverage.py
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        return f.read()


def topics():
    found = set()
    for name in os.listdir(os.path.join(ROOT, "man")):
        if name.endswith(".txt"):
            first = read(os.path.join("man", name)).split("\n", 1)[0]
            found.update(first[2:].split())
    return found


def wanted():
    need = set()
    lexer = read("src/front/lexer.c")
    need.update(re.findall(r'\{"([a-z_]+)",\s*\d+,\s*TOK_', lexer))
    header = read("include/resolver/natives.h")
    native_list = header[header.index("# define NATIVE_LIST(X)"):header.index("# define NATIVE_MODULES")]
    need.update(re.findall(r"X\((\w+),", native_list))
    modules = header[header.index("# define NATIVE_MODULES(M)"):header.index("# define NATIVE_MEMBERS")]
    need.update(re.findall(r"M\((\w+)\)", modules.split("\n", 1)[1]))
    members = header[header.index("# define NATIVE_MEMBERS(F, C)"):header.index("# define NATIVE_MODULE_ENUM")]
    for mod, name in re.findall(r"\b[FC]\((\w+), (\w+)", members.split("\n", 1)[1]):
        need.add(f"{mod}.{name}")
    tables = {
        "src/types/number.c": {"g_number_methods": "number"},
        "src/types/string.c": {"g_string_methods": "string"},
        "src/types/list.c": {"g_list_methods": "list", "g_tuple_methods": "tuple"},
        "src/types/dict.c": {"g_dict_methods": "dict", "g_set_methods": "set"},
        "src/types/error.c": {"g_error_methods": "error"},
        "src/types/file.c": {"g_file_methods": "file"},
    }
    for path, names in tables.items():
        text = read(path)
        for table, type_name in names.items():
            body = text[text.index(table + "[] = {"):]
            body = body[:body.index("};")]
            for method in re.findall(r'\{"(\w+)",', body):
                need.add(f"{type_name}.{method}")
    common = read("src/runtime/methods.c")
    body = common[common.index("g_common_methods[] = {"):]
    need.update(re.findall(r'\{"(\w+)",', body[:body.index("};")]))
    return need


def main():
    missing = sorted(wanted() - topics())
    if missing:
        print("man coverage: no page for " + ", ".join(missing))
        sys.exit(1)
    print(f"man coverage: every name has its page ({len(wanted())} names)")


main()
