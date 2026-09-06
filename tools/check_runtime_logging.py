"""Static logging-policy check; no Workbench installation required.

Runtime NORMAL/DEBUG Print calls must be inside IA_Log.IsDebugEnabled() blocks.
Use IA_Log.Info for intentional production audit records. Warnings/errors remain
unfiltered. Explicit Workbench test helpers are excluded, not gameplay code.
"""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEST_HELPERS = {"IA_SelectionTestPlacer.c", "IA_InfrastructureTestPlacer.c"}
LITERALS_COMMENTS = re.compile(r'"(?:\\.|[^"\\])*"|//[^\n]*|/\*[\s\S]*?\*/')


def mask_source(source: str) -> str:
    return LITERALS_COMMENTS.sub(
        lambda match: re.sub(r"[^\n]", " ", match[0]), source
    )


def closing(masked: str, start: int, left: str, right: str) -> int:
    depth = 1
    for index in range(start + 1, len(masked)):
        if masked[index] == left:
            depth += 1
        elif masked[index] == right:
            depth -= 1
            if depth == 0:
                return index
    raise ValueError(f"Unclosed {left} at offset {start}")


def debug_blocks(source: str) -> list[tuple[int, int]]:
    masked = mask_source(source)
    blocks = []
    for match in re.finditer(r"if\s*\(IA_Log\.IsDebugEnabled\(\)\)\s*\{", masked):
        start = match.end() - 1
        blocks.append((start, closing(masked, start, "{", "}")))
    return blocks


def print_calls(source: str):
    masked = mask_source(source)
    for match in re.finditer(r"\bPrint(?:Format)?\s*\(", masked):
        end = closing(masked, match.end() - 1, "(", ")") + 1
        level = re.search(r"LogLevel\.(\w+)\s*\)$", masked[match.start():end])
        yield match.start(), end, level[1] if level else "BARE"


def violations(source: str) -> list[int]:
    blocks = debug_blocks(source)
    errors = []
    for start, _, level in print_calls(source):
        guarded = any(left < start < right for left, right in blocks)
        if level in {"NORMAL", "DEBUG", "BARE"} and not guarded:
            errors.append(start)
        elif level in {"WARNING", "ERROR", "FATAL"} and guarded:
            errors.append(start)
    return errors


def main() -> int:
    errors = []
    count = 0
    for path in sorted((ROOT / "Scripts/Game").rglob("*.c")):
        if path.name in TEST_HELPERS or path.name == "IA_Log.c":
            continue
        source = path.read_text(encoding="utf-8-sig")
        count += 1
        for start in violations(source):
            line = source.count("\n", 0, start) + 1
            errors.append(f"{path.relative_to(ROOT)}:{line}: logging policy violation "
                          "(gate traces, not warnings/errors)")
    if errors:
        print("\n".join(errors))
        return 1
    print(f"PASS: {count} runtime scripts; routine Print calls are debug-gated.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
