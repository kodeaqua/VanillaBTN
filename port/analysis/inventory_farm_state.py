"""Audit native farm objects with Clang; normal builds use the checked inventory.

Run with a native compile_commands.json. Review regenerated metadata, especially
the read-only-in-use local cue tables, before accepting changed source hashes.
No game assets or matching toolchain are needed.
"""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess


ROOT = Path(__file__).resolve().parents[2]
FARM = ROOT / "src/overlay/11_GF_FARM"
OUTPUT = ROOT / "port/overlay/farm_state"


def digest(path):
    return hashlib.sha256(path.read_bytes().replace(b"\r\n", b"\n")).hexdigest()


def header_digest():
    paths = set((ROOT / "src").rglob("*.h"))
    paths.update((ROOT / "port/compat").rglob("*.h"))
    paths.update((ROOT / "port/compat").rglob("*.inc"))
    data = "".join(f"{p.relative_to(ROOT).as_posix()} {digest(p)}\n"
                   for p in sorted(paths, key=lambda p: p.relative_to(ROOT).as_posix()))
    return hashlib.sha256(data.encode()).hexdigest()


def inspect(source, entry):
    args = shlex.split(entry["command"].replace("\\", "/"))
    for flag in ("-o", "-MF", "-MT"):
        if flag in args:
            at = args.index(flag)
            del args[at:at + 2]
    at = args.index("-c")
    args[at + 1] = source.as_posix()
    args = [a for a in args if a not in ("-c", "-MD")]
    args += ["-Xclang", "-ast-dump=json", "-fsyntax-only"]
    result = subprocess.run(args, cwd=entry["directory"], capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors="replace"))
    ast = json.loads(result.stdout)
    objects = dict()
    local_tables = []
    for node in ast.get("inner", []):
        if (node.get("kind") == "VarDecl" and node.get("storageClass") != "extern"
                and node.get("loc", {}).get("offset") is not None):
            objects[node["name"]] = node
        if node.get("kind") != "FunctionDecl":
            continue
        locals_in_function = []

        def collect(part):
            if part.get("kind") == "VarDecl" and part.get("storageClass") == "static":
                locals_in_function.append(part)
            for child in part.get("inner", []):
                collect(child)

        collect(node)
        for local in locals_in_function:
            if local["name"] != "steps" or not re.fullmatch(r"FarmCueStep\[\d+\]", local["type"]["qualType"]):
                raise RuntimeError(f"Unreviewed local static: {source}: {local['name']}")

            def references(part, caller=None, argument=None):
                if part.get("kind") == "CallExpr":
                    def function_name(expr):
                        declaration = expr.get("referencedDecl", {})
                        if declaration.get("kind") == "FunctionDecl":
                            return declaration["name"]
                        for child in expr.get("inner", []):
                            found = function_name(child)
                            if found:
                                return found
                        return None
                    caller = function_name(part["inner"][0])
                    for index, child in enumerate(part.get("inner", [])):
                        references(child, caller, index)
                    return
                if part.get("kind") == "DeclRefExpr" and part.get("referencedDecl", {}).get("id") == local["id"]:
                    if caller != "farm_cue_step_run" or argument != 2:
                        raise RuntimeError(f"Unreviewed cue-table access: {source}: {node['name']}")
                for child in part.get("inner", []):
                    references(child, caller, argument)

            references(node)
            local_tables.append({"function": node["name"], "object": local["name"],
                                 "type": local["type"]["qualType"],
                                 "consumer": "farm_cue_step_run (read-only table)"})
    return source, list(objects), local_tables


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compile_commands", type=Path)
    parser.add_argument("--check", action="store_true")
    options = parser.parse_args()
    database = json.loads(options.compile_commands.read_text(encoding="utf-8"))
    sources = sorted(FARM.rglob("*.c"))
    jobs = []
    for source in sources:
        relative = source.relative_to(FARM).as_posix()
        identifier = re.sub(r"[^A-Za-z0-9_]", "_", relative)
        matches = [e for e in database if e["file"].replace("\\", "/").endswith(
            ("/11_GF_FARM/" + relative, "/native_farm/" + identifier + ".c"))]
        if len(matches) != 1:
            raise RuntimeError(f"Expected one native compile command for {relative}")
        jobs.append((source, matches[0]))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        results = list(pool.map(lambda job: inspect(*job), jobs))
    expected = {}
    manifest = ["# Clang-audited native farm inventory; see ADR-073.\n",
                f'hm_farm_state_headers("{header_digest()}")\n']
    locals_report = {}
    for source, names, local_tables in results:
        relative = source.relative_to(FARM).as_posix()
        identifier = re.sub(r"[^A-Za-z0-9_]", "_", relative)
        manifest.append(f'hm_farm_state_unit("{relative}" "{digest(source)}")\n')
        if names:
            text = ['/* Real objects only; const objects are filtered by their C type. */\n',
                    '#include "overlay/state_inventory.h"\n',
                    f'hm_overlay_inventory hm_farm_inventory_{identifier}(void) {{\n',
                    '    static const hm_overlay_span spans[] = {\n']
            text += [f'        HM_OVERLAY_OBJECT({name}),\n' for name in names]
            text += ['    };\n', '    return (hm_overlay_inventory){spans, sizeof spans / sizeof spans[0]};\n', '}\n']
            expected[identifier + ".inc"] = "".join(text)
        if local_tables:
            locals_report[relative] = local_tables
    expected["manifest.cmake"] = "".join(manifest)
    expected["local_tables.json"] = json.dumps(locals_report, indent=2) + "\n"
    if options.check:
        for name, content in expected.items():
            if not (OUTPUT / name).exists() or (OUTPUT / name).read_text(encoding="utf-8") != content:
                raise RuntimeError(f"Stale farm inventory: {name}")
        actual = {p.name for p in OUTPUT.glob("*.inc")}
        if actual != {n for n in expected if n.endswith(".inc")}:
            raise RuntimeError("Unexpected farm inventory files")
    else:
        OUTPUT.mkdir(parents=True, exist_ok=True)
        for name, content in expected.items():
            (OUTPUT / name).write_text(content, encoding="utf-8", newline="\n")
    print(f"Audited {len(results)} units, {sum(len(n) for _, n, _ in results)} objects, "
          f"{sum(len(v) for v in locals_report.values())} read-only-in-use local cue tables")


if __name__ == "__main__":
    main()
