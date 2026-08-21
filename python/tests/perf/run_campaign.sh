#!/bin/bash
# Both arms of the campaign, end to end: f32 ladder+probes, rebuild, f64
# ladder+probes, rebuild back. ~1.5 h, mostly unattended.
#
#   ./run_campaign.sh            # both arms
#   ./run_campaign.sh f32        # one arm
#
# The GPU must be idle: run_ladder.py and run_probes.py both abort above 5 %
# utilisation, because a competing process leaves per-operation averages intact
# while destroying the duty cycle and the wall clock. Close any notebook first.
#
# The arm is selected by ONE line in the kernel header. Nothing else differs --
# same commands, same parameters -- and env.json records which arm produced each
# result set, so a stale build cannot silently mislabel a campaign.
set -euo pipefail

REPO=/home/ferjao_k/aare
HDR=$REPO/include/aare/clusterfinder_kernel.cuh
PERF=$REPO/python/tests/perf
PY=${PY:-/home/ferjao_k/.conda/envs/py/bin/python3.11}

set_ped() {                       # set_ped float|double
    sed -i "s/^using DEVICE_PED_TYPE = .*;/using DEVICE_PED_TYPE = $1;/" "$HDR"
    echo "=== rebuilding with DEVICE_PED_TYPE = $1 ==="
    cmake --build "$REPO/build" -j 16 2>&1 | grep -E "Built target _aare_cuda|error" || {
        echo "BUILD FAILED"; exit 1; }
    got=$(cd "$PERF" && $PY -c "import sys;sys.path.insert(0,'.');import common;print(common.device_ped_type())")
    [ "$got" = "$1" ] || { echo "header reports '$got', expected '$1' — aborting"; exit 1; }
    echo "verified: $got"
}

run_arm() {
    cd "$PERF"
    echo "=== ladder ==="; $PY run_ladder.py
    echo "=== probes ==="; $PY run_probes.py
}

arm=${1:-both}
if [ "$arm" = "f32" ] || [ "$arm" = "both" ]; then
    set_ped float
    run_arm
fi
if [ "$arm" = "f64" ] || [ "$arm" = "both" ]; then
    set_ped double
    run_arm
    # f32 is the shipping default: never leave the tree on the f64 build, or the
    # next person's notebook silently runs a 40 % slower kernel at 9x9.
    set_ped float
fi
echo "=== CAMPAIGN COMPLETE ==="
