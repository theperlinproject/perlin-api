#!/usr/bin/env python3
"""Writes a file as a C++ byte array: embed_file.py <input> <output.hpp> <symbol>"""
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 4:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    data = Path(sys.argv[1]).read_bytes()
    lines = []
    for i in range(0, len(data), 24):
        lines.append('    ' + ', '.join(str(b) for b in data[i:i + 24]) + ',')
    body = '\n'.join(lines)
    out = Path(sys.argv[2])
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text('#pragma once\n#include <cstdint>\n\n'
                   f'inline constexpr uint8_t {sys.argv[3]}[] = {{\n{body}\n}};\n', encoding='utf-8')
    return 0


if __name__ == '__main__':
    sys.exit(main())
