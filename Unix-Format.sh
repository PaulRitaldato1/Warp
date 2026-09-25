#!/usr/bin/env bash
# Reformats every engine, testbed and shader source with .clang-format.
# Skips build/ so fetched dependencies are left alone.
# Pass --check to fail without writing, for CI or a pre-commit hook.

set -euo pipefail

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "clang-format not found. Set CLANG_FORMAT to its path."
    exit 1
fi

MODE=(-i)
if [[ "${1:-}" == "--check" ]]; then
    MODE=(--dry-run -Werror)
fi

mapfile -t FILES < <(find Warp/Engine Warp/TestBed Warp/EntryPoint Warp/Shaders \
    \( -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.c' -o -name '*.hlsl' \) \
    2>/dev/null)

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "No source files found. Run from the repository root."
    exit 1
fi

"$CLANG_FORMAT" "${MODE[@]}" "${FILES[@]}"
echo "Formatted ${#FILES[@]} files."
