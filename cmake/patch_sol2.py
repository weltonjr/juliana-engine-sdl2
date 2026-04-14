#!/usr/bin/env python3
"""
Patch sol2 v3.3.0: optional<T&>::emplace incorrectly calls this->construct()
which only exists in the non-reference optional base class (optional_operations_base).
This triggers a hard compile error on Emscripten's Clang and newer Clang versions.

The closing-brace context uniquely identifies the reference-specialization emplace
vs the non-reference one which has 'return value();' before the brace.
"""

import sys
import pathlib

if len(sys.argv) < 2:
    print("Usage: patch_sol2.py <path/to/optional_implementation.hpp>")
    sys.exit(1)

path = pathlib.Path(sys.argv[1])
content = path.read_text()

TAB = "\t"
NL  = "\n"

old = (TAB * 3 + "this->construct(std::forward<Args>(args)...);" + NL +
       TAB * 2 + "}")

new = (TAB * 3 + "m_value = std::addressof("
                   "std::get<0>(std::forward_as_tuple(std::forward<Args>(args)...)));" + NL +
       TAB * 3 + "return *m_value;" + NL +
       TAB * 2 + "}")

if old in content:
    path.write_text(content.replace(old, new))
    print("sol2 patch applied: optional<T&>::emplace fixed")
else:
    print("sol2 patch: pattern not found (already patched or different version)")
