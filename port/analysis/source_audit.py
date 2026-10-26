"""Conservative source inventory; not a C preprocessor or a linker proof."""

from __future__ import annotations

import argparse
from collections import defaultdict
from dataclasses import dataclass
import json
from pathlib import Path
import re


TOKEN = re.compile(
    r'(?P<skip>/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|'
    r'^[ \t]*\#(?:\\\r?\n|[^\n])*)'
    r'|(?P<name>[A-Za-z_]\w*)'
    r'|(?P<number>0[xX][0-9a-fA-F]+[uUlL]*|\d+(?:\.\d*)?[uUlLfF]*)'
    r'|(?P<symbol>[^\s])',
    re.DOTALL | re.MULTILINE,
)


@dataclass(frozen=True)
class Token:
    value: str
    line: int
    offset: int


def tokens(source: str) -> list[Token]:
    result = []
    end = 0
    line = 1
    for match in TOKEN.finditer(source):
        line += source[end:match.start()].count("\n")
        if match.lastgroup != "skip":
            result.append(Token(match.group(), line, match.start()))
        line += match.group().count("\n")
        end = match.end()
    return result


def functions(source: str) -> list[dict]:
    """Recognize ordinary file-scope function bodies; skip initializer braces."""
    result = []
    declaration = []
    depth = 0
    body = False
    for token in tokens(source):
        if token.value == "{":
            if depth == 0:
                words = [item.value for item in declaration]
                body = "=" not in words and "(" in words and words[-1:] == [")"]
                if body:
                    opening = words.index("(")
                    if opening > 0 and re.fullmatch(r"[A-Za-z_]\w*", words[opening - 1]):
                        name = declaration[opening - 1]
                        result.append({"name": name.value, "line": name.line,
                                       "offset": name.offset, "static": "static" in words})
            depth += 1
        elif token.value == "}":
            depth -= 1
            if depth == 0 and body:
                declaration = []
                body = False
        elif depth == 0:
            if token.value == ";":
                declaration = []
            else:
                declaration.append(token)
    return result


def owner(path: str) -> str:
    parts = path.split("/")
    return parts[2] if parts[:2] == ["src", "overlay"] else "main"


def audit(root: Path) -> dict:
    sources = sorted((root / "src").rglob("*.c"))
    headers = sorted((root / "src").rglob("*.h"))
    contents = {path: path.read_text(encoding="utf-8") for path in sources + headers}
    definitions = []
    all_defined = set()
    local_macros = set()
    includes = defaultdict(set)
    for path, source in contents.items():
        relative = path.relative_to(root).as_posix()
        local_macros.update(re.findall(r"^\s*#define\s+(\w+)", source, re.MULTILINE))
        for header in re.findall(r"^\s*#include\s+<([^>]+)>", source, re.MULTILINE):
            includes[header].add(relative)
        if path.suffix == ".c":
            for definition in functions(source):
                definitions.append({**definition, "file": relative, "owner": owner(relative)})
                all_defined.add(definition["name"])

    by_name = defaultdict(list)
    for definition in definitions:
        if not definition["static"]:
            by_name[definition["name"]].append(definition)
    collisions = {name: locations for name, locations in sorted(by_name.items())
                  if len(locations) > 1}
    definition_offsets = {(item["file"], item["offset"]) for item in definitions}
    candidates = defaultdict(list)
    gte_macros = defaultdict(list)
    excluded = {"if", "while", "for", "switch", "sizeof", "return", "_Static_assert"}
    for path in sources:
        relative = path.relative_to(root).as_posix()
        stream = tokens(contents[path])
        for first, second in zip(stream, stream[1:]):
            if second.value != "(" or not re.fullmatch(r"[A-Za-z_]\w*", first.value):
                continue
            if (relative, first.offset) in definition_offsets:
                continue
            location = {"file": relative, "line": first.line}
            if first.value.startswith("gte_"):
                gte_macros[first.value].append(location)
            elif first.value not in all_defined | local_macros | excluded:
                candidates[first.value].append(location)

    overlays = []
    for path in sorted((root / "src" / "overlay").glob("*/CMakeLists.txt")):
        match = re.search(r"psx_overlay\((\S+)\s+FILE\s+(\S+)\s+REGION\s+(\S+)",
                          path.read_text(encoding="utf-8"))
        if match:
            overlays.append({"name": match[1], "binary": match[2], "region": match[3]})
    for item in definitions:
        item.pop("offset")
    return {
        "schema": 1,
        "limitations": [
            "Lexical candidates only: no preprocessing, conditional selection, or SDK inputs.",
            "Function definitions only; global data collisions and unusual declarators need linker analysis.",
            "Functions generated by macro expansion are not recognized.",
            "External call candidates can include casts, callbacks and declarations; not an SDK implementation list.",
            "Local macros are excluded except gte_ invocations, which are reported separately.",
        ],
        "counts": {"sources": len(sources), "headers": len(headers),
                   "functions": len(definitions), "external_functions": len(by_name),
                   "function_collision_candidates": len(collisions),
                   "external_call_names": len(candidates), "gte_macro_names": len(gte_macros)},
        "overlays": overlays,
        "system_includes": {name: sorted(paths) for name, paths in sorted(includes.items())},
        "functions": definitions,
        "function_collision_candidates": collisions,
        "external_call_candidates": dict(sorted(candidates.items())),
        "gte_macro_invocations": dict(sorted(gte_macros.items())),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.root.resolve())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(report["counts"], sort_keys=True))


if __name__ == "__main__":
    main()
