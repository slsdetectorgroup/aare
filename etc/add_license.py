#!/usr/bin/env python3
import argparse
import fnmatch
import os
from pathlib import Path

CPP_PATTERNS = ["*.h", "*.hpp", "*.cpp"]
PY_PATTERNS = ["*.py"]
CMAKE_PATTERNS = ["CMakeLists.txt"]

FILE_PATTERNS = CPP_PATTERNS + PY_PATTERNS + CMAKE_PATTERNS
LICENSE_TEXT = "SPDX-License-Identifier: MPL-2.0"


def get_comment_prefix(filename: str) -> str | None:
    if any(fnmatch.fnmatch(filename, p) for p in CPP_PATTERNS):
        return "// "
    if any(fnmatch.fnmatch(filename, p) for p in (PY_PATTERNS + CMAKE_PATTERNS)):
        return "# "
    return None


def matches_pattern(filename: str) -> bool:
    return any(fnmatch.fnmatch(filename, p) for p in FILE_PATTERNS)


def process_file(filepath: Path) -> bool:
    filename = filepath.name
    prefix = get_comment_prefix(filename)
    if not prefix:
        return False

    license_line = f"{prefix}{LICENSE_TEXT}\n"

    try:
        with filepath.open("r", encoding="utf-8") as f:
            lines = f.readlines()
    except Exception as e:
        print(f"⚠️ Error reading {filepath}: {e}")
        return False

    # Skip if SPDX already present anywhere in the file
    if any("SPDX-License-Identifier" in line for line in lines):
        return False

    insert_index = 0

    # For Python, keep shebang on the very first line
    if filename.endswith(".py") and lines:
        if lines[0].startswith("#!"):
            insert_index = 1

    lines.insert(insert_index, license_line)

    try:
        with filepath.open("w", encoding="utf-8") as f:
            f.writelines(lines)
    except Exception as e:
        print(f"⚠️ Error writing {filepath}: {e}")
        return False

    return True


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Add SPDX-License-Identifier: MPL-2.0 to source files."
    )
    parser.add_argument(
        "path",
        help="Root directory to recursively process "
             "(*.h, *.cpp, *.py, and CMakeLists.txt).",
    )

    args = parser.parse_args()

    root_path = Path(args.path).expanduser().resolve()

    if not root_path.exists():
        print(f"Error: Path does not exist: {root_path}")
        raise SystemExit(1)

    if not root_path.is_dir():
        print(f"Error: Path is not a directory: {root_path}")
        raise SystemExit(1)

    print(f"Processing directory: {root_path}")
    modified = 0

    for dirpath, _, files in os.walk(root_path):
        dirpath = Path(dirpath)
        for name in files:
            if matches_pattern(name):
                fullpath = dirpath / name
                if process_file(fullpath):
                    print(f"✔ Added SPDX: {fullpath}")
                    modified += 1

    print(f"\nDone. Updated {modified} file(s).")


if __name__ == "__main__":
    main()