#!/usr/bin/env python3
"""Register pressure, spills and occupancy for every ClusterFinder kernel.

Reads the *compiled* extension — no rebuild and no source parsing:

    cuobjdump -res-usage <lib.so>

then applies the sm_89 occupancy arithmetic. Reproduces the figures on slides 7
and 8 of docs/cf_cuda_performance.pptx, and cross-checks against
cudaOccupancyMaxActiveBlocksPerMultiprocessor (same blocks/SM).

    python kernel_resources.py                    # shipping build, 16x16 blocks
    python kernel_resources.py --blocks 64 256 1024   # the block-size sweep
    python kernel_resources.py --lib path/to/other.so

Two things to know when reading the output:

1. **Register count depends on the cluster payload type.** At 9x9 it is 96
   (float) / 120 (double) / 128 (int). The Python bindings register the *int*
   variants, so those are the rows to quote.

2. **Register count also depends on DEVICE_PED_TYPE**, at 3x3. The f64 pedestal
   costs 9 extra registers there — 47 vs 38 — which is enough to lose a block per
   SM and drop occupancy from 100 % to 83 %. At 9x9 nothing moves: the limiter is
   the clusterData[CSX][CSY] staging array, not the pedestal accumulators. Always
   record which build a number came from; `common.device_ped_type()` reports it.

SHARED reads 0 in the ELF because the finder passes shared memory dynamically at
launch, so it is recomputed here with the launcher's own formula
(ClusterFinderCUDA.hpp): (BLOCK_X + 2*col_radius) * (BLOCK_Y + 2*row_radius) *
sizeof(COMPUTE_TYPE).
"""
from __future__ import annotations

import argparse
import math
import re
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
DEFAULT_LIB = REPO / "build/aare/_aare_cuda.cpython-311-x86_64-linux-gnu.so"

# --- RTX 4090 / sm_89, from cudaDeviceProp ---------------------------------
REGS_PER_SM = 65536
MAX_THREADS_PER_SM = 1536
MAX_BLOCKS_PER_SM = 24
SMEM_PER_SM = 102400          # opt-in maximum; 48 KB is the default carveout
WARP = 32
REG_ALLOC_GRANULARITY = 256   # registers are allocated per warp, rounded up

KERNEL_RE = re.compile(r"\s*Function (\S+):")
USAGE_RE = re.compile(r"REG:(\d+) STACK:(\d+) SHARED:(\d+) LOCAL:(\d+)")
TEMPLATE_RE = re.compile(
    r"Cluster<(\w+), \(unsigned char\)(\d+), \(unsigned char\)(\d+)")


def demangle(name: str) -> str:
    try:
        import cxxfilt
        return cxxfilt.demangle(name)
    except Exception:
        out = subprocess.run(["c++filt", name], capture_output=True, text=True)
        return out.stdout.strip() or name


def read_res_usage(lib: Path) -> list[tuple[str, int, int, int, int]]:
    """[(mangled_name, regs, stack, shared, local), ...] straight from the ELF."""
    proc = subprocess.run(["cuobjdump", "-res-usage", str(lib)],
                          capture_output=True, text=True)
    if proc.returncode != 0:
        sys.exit(f"cuobjdump failed:\n{proc.stderr}")
    rows, cur = [], None
    for line in proc.stdout.splitlines():
        m = KERNEL_RE.match(line)
        if m:
            cur = m.group(1)
            continue
        m = USAGE_RE.search(line)
        if m and cur:
            rows.append((cur, *map(int, m.groups())))
            cur = None
    return rows


def occupancy(regs: int, smem_per_block: int, block: int):
    """Blocks/SM, warps/SM, occupancy %, and which resource binds."""
    warps_per_block = math.ceil(block / WARP)
    per_warp = math.ceil(regs * WARP / REG_ALLOC_GRANULARITY) * REG_ALLOC_GRANULARITY
    limits = {
        "registers": (REGS_PER_SM // per_warp) // warps_per_block,
        "threads/SM": MAX_THREADS_PER_SM // block,
        "shared mem": SMEM_PER_SM // smem_per_block if smem_per_block else 1 << 20,
        "blocks/SM": MAX_BLOCKS_PER_SM,
    }
    blocks = min(limits.values())
    binder = min(limits, key=lambda k: (limits[k], k))
    warps = blocks * warps_per_block
    return blocks, warps, 100.0 * warps / (MAX_THREADS_PER_SM // WARP), binder


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lib", type=Path, default=DEFAULT_LIB)
    ap.add_argument("--blocks", type=int, nargs="+", default=[256],
                    help="threads per block (default 256 = the 16x16 the finder launches)")
    ap.add_argument("--all-types", action="store_true",
                    help="also show the float/double cluster payloads (bindings use int)")
    args = ap.parse_args()

    if not args.lib.exists():
        sys.exit(f"no such library: {args.lib}\nBuild it first: cmake --build build -j16")

    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import common
        build = common.device_ped_type()
    except Exception:
        build = "unknown"

    kernels = []
    for name, regs, stack, shared, local in read_res_usage(args.lib):
        d = demangle(name)
        if "find_clusters_in_single_frame" not in d:
            continue
        m = TEMPLATE_RE.search(d)
        if not m:
            continue
        ctype, sx, sy = m.group(1), int(m.group(2)), int(m.group(3))
        if not args.all_types and ctype != "int":
            continue
        pipeline = "opt2" if "device_opt2" in d else "current"
        kernels.append((pipeline, sx, sy, ctype, regs, stack + local))

    kernels.sort(key=lambda k: (k[0] != "current", k[1]))

    print(f"library      {args.lib}")
    print(f"build        DEVICE_PED_TYPE = {build}")
    print(f"payload      {'all cluster types' if args.all_types else 'int (what the bindings register)'}")
    print()

    for block in args.blocks:
        side = int(math.isqrt(block))
        print(f"--- {side}x{side} blocks · {block} threads ---")
        print(f"{'kernel':28} {'regs':>5} {'spill':>6} {'smem/blk':>9} "
              f"{'blk/SM':>7} {'warps':>6} {'occupancy':>10}  limited by")
        print("-" * 92)
        for pipeline, sx, sy, ctype, regs, spill in kernels:
            # the launcher's own formula; COMPUTE_TYPE is float in every build
            smem = (side + sy - 1) * (side + sx - 1) * 4
            blocks, warps, occ, binder = occupancy(regs, smem, block)
            label = f"[{pipeline}] {sx}x{sy} Cluster<{ctype}>"
            note = "  <- will not launch" if blocks == 0 else ""
            print(f"{label:28} {regs:5} {spill:6} {smem:8} B {blocks:7} {warps:6} "
                  f"{occ:9.1f}% {binder}{note}")
        print()

    print(f"sm_89: {REGS_PER_SM:,} regs/SM · {MAX_THREADS_PER_SM:,} threads/SM · "
          f"{MAX_BLOCKS_PER_SM} blocks/SM · {REG_ALLOC_GRANULARITY}-reg granularity")
    print("Spills must be 0. A non-zero STACK/LOCAL means ptxas gave up and went "
          "to local memory,\nwhich costs far more than the occupancy it buys back.")


if __name__ == "__main__":
    main()
