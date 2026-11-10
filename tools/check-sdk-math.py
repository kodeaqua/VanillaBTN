"""Compare native SDK math with reference SDK microcalls, without running a game.

Optional developer dependency: unicorn==2.1.4. Neither Unicorn nor the reference
ELF/SDK is needed by normal native builds. Only numerical function results are
generated; never copy SDK instructions, data files or headers into the repo.
"""

import argparse
from functools import lru_cache
from pathlib import Path
import re
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, default=Path("build/program.elf"))
    parser.add_argument("--map", type=Path, default=Path("build/program.map"))
    parser.add_argument("--native", type=Path, default=Path("build/native/Debug/sdk_math_test.exe"))
    parser.add_argument("--write-table", action="store_true")
    args = parser.parse_args()
    try:
        from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_LITTLE_ENDIAN, UC_HOOK_CODE
        from unicorn import mips_const as reg
    except ImportError:
        parser.error("Install the optional checker dependency: python -m pip install unicorn==2.1.4")

    symbols = {}
    for line in args.map.read_text().splitlines():
        match = re.fullmatch(r"\s+0x([0-9a-fA-F]+)\s+(ratan2|csqrt|Lzc)\s*", line)
        if match:
            symbols[match[2]] = int(match[1], 16)
    if set(symbols) != {"ratan2", "csqrt", "Lzc"}:
        parser.error("Reference map must expose ratan2, csqrt and Lzc")
    data = args.elf.read_bytes()
    if data[:6] != b"\x7fELF\x01\x01":
        parser.error("Reference must be a little-endian ELF32 build")
    machine = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 | UC_MODE_LITTLE_ENDIAN)
    # Unicorn's CPU translates KSEG0 accesses to low physical RAM; also map
    # the cached alias for its memory API. All execution stays inside the VM.
    machine.mem_map(0, 0x200000)
    machine.mem_map(0x80000000, 0x200000)
    offset = struct.unpack_from("<I", data, 28)[0]
    stride, count = struct.unpack_from("<HH", data, 42)
    for i in range(count):
        kind, start, address, _, size, _, _, _ = struct.unpack_from("<8I", data, offset + i * stride)
        if kind == 1 and size and 0x80000000 <= address and address + size <= 0x80200000:
            machine.mem_write(address, data[start:start + size])
            machine.mem_write(address & 0x1FFFFFFF, data[start:start + size])

    def leading_sign_bits(cpu, address, size, context):
        # Lzc uses a COP2 operation not executed by this CPU model. This hook
        # supplies that documented primitive, not a replacement sqrt/atan.
        value = cpu.reg_read(reg.UC_MIPS_REG_A0)
        if value & 0x80000000:
            value ^= 0xFFFFFFFF
        cpu.reg_write(reg.UC_MIPS_REG_V0, 32 - value.bit_length())
        cpu.reg_write(reg.UC_MIPS_REG_PC, cpu.reg_read(reg.UC_MIPS_REG_RA))

    machine.hook_add(UC_HOOK_CODE, leading_sign_bits, begin=symbols["Lzc"], end=symbols["Lzc"])

    @lru_cache(maxsize=None)
    def reference(name, a, b=0):
        machine.reg_write(reg.UC_MIPS_REG_A0, a & 0xFFFFFFFF)
        machine.reg_write(reg.UC_MIPS_REG_A1, b & 0xFFFFFFFF)
        machine.reg_write(reg.UC_MIPS_REG_SP, 0x801FF000)
        machine.reg_write(reg.UC_MIPS_REG_RA, 0x80000000)
        machine.emu_start(symbols[name], 0x80000000, count=100000)
        if machine.reg_read(reg.UC_MIPS_REG_PC) != 0x80000000:
            raise RuntimeError("Reference math microcall did not return")
        value = machine.reg_read(reg.UC_MIPS_REG_V0)
        return value if value < 0x80000000 else value - 0x100000000

    if args.write_table:
        values = [reference("ratan2", i, 1024) for i in range(1025)]
        output = Path(__file__).resolve().parents[1] / "native/src/sdk_atan_table.inc"
        text = "/* Quantized integer-angle function outputs; see tools/check-sdk-math.py. */\n"
        text += "\n".join("    " + ", ".join(map(str, values[i:i + 16])) + ","
                          for i in range(0, len(values), 16)) + "\n"
        output.write_text(text, newline="\n")
        print("Generated 1,025 numerical integer-angle results")
        return

    lines = subprocess.check_output([str(args.native.resolve()), "--reference"], text=True).splitlines()
    if len(lines) != 129 * 129:
        raise RuntimeError("Native checker did not emit the complete analog magnitude grid")
    for expected_index, line in enumerate(lines):
        x, y, angle, root = map(int, line.split())
        if (x, y) != divmod(expected_index, 129):
            raise RuntimeError("Native analog grid is incomplete or out of order")
        expected_angle = reference("ratan2", x, y)
        expected_root = reference("csqrt", (x * x + y * y) << 12)
        if (angle, root) != (expected_angle, expected_root):
            raise RuntimeError(f"Math mismatch at ({x}, {y}): {(angle, root)} != {(expected_angle, expected_root)}")
    print("All 16,641 analog magnitudes match reference atan/CORDIC results")


if __name__ == "__main__":
    main()
