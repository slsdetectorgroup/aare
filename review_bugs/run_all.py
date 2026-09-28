"""Run every bug demo in this folder and summarise which ones reproduce.

    PYTHONPATH=build python review_bugs/run_all.py
"""
import subprocess
import sys
from pathlib import Path

here = Path(__file__).parent
results = []
for script in sorted(here.glob("[0-9][0-9]_*.py")):
    print(f"\n{'=' * 72}\n{script.name}\n{'=' * 72}", flush=True)
    rc = subprocess.run([sys.executable, str(script)]).returncode
    results.append((script.name, rc))

print(f"\n{'=' * 72}\nsummary")
for name, rc in results:
    print(f"  {'REPRODUCED' if rc == 1 else 'ok' if rc == 0 else 'ERROR'}  {name}")
