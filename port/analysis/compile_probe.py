"""Probe every original C translation unit with native Clang; never link or run it."""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import re
import subprocess


def probe(root: Path, compiler: Path, output: Path, jobs: int) -> dict:
    sources = sorted((root / "src").rglob("*.c"))
    include_dirs = sorted({path.parent for path in (root / "src").rglob("*.h")})
    arguments = [str(compiler), "-std=c11", "-fsyntax-only", "-ferror-limit=5",
                 "-Wall", "-Wextra", "-fwrapv", "-fno-strict-aliasing",
                 "-fno-delete-null-pointer-checks", "-DHMBTN_PORT=1",
                 "-include", str(root / "port/compat/game.h"),
                 "-I" + str(root / "port"), "-I" + str(root / "port/compat/psyq"),
                 "-I" + str(root / "src")]
    arguments.extend("-I" + str(path) for path in include_dirs)

    def compile_one(source: Path) -> dict:
        relative = source.relative_to(root).as_posix()
        process = subprocess.run(arguments + [relative], cwd=root, capture_output=True,
                                 encoding="utf-8", errors="replace", timeout=60, check=False)
        diagnostic = process.stderr.replace(str(root), "<repo>").replace(root.as_posix(), "<repo>")
        return {"file": relative, "returncode": process.returncode,
                "warnings": len(re.findall(r": warning:", diagnostic)),
                "diagnostics": diagnostic}

    # Host-side analysis only; these processes never execute game logic.
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        results = list(pool.map(compile_one, sources))
    version = subprocess.run([str(compiler), "--version"], capture_output=True, text=True,
                             timeout=10, check=True).stdout.splitlines()[0]
    report = {
        "schema": 1,
        "compiler": version,
        "limitations": ["Syntax-only; successful files are not necessarily linked or safe to execute.",
                        "Only the first five errors per translation unit are requested.",
                        "No SDK, disc data, source mutation, or game execution is involved."],
        "counts": {"sources": len(results),
                   "passed": sum(item["returncode"] == 0 for item in results),
                   "failed": sum(item["returncode"] != 0 for item in results),
                   "warnings": sum(item["warnings"] for item in results)},
        "results": results,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if not args.compiler.is_file():
        parser.error("--compiler must name an existing Clang executable")
    report = probe(args.root.resolve(), args.compiler.resolve(), args.output, args.jobs)
    print(json.dumps(report["counts"], sort_keys=True))
    # Failing translation units are expected during bring-up; command failures still raise.


if __name__ == "__main__":
    main()
