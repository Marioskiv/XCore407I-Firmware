#!/usr/bin/env python3
"""Verify pin map CSV files are up to date.

Steps:
 1. Regenerate extracted variant map (extract_pin_map.py)
 2. Regenerate profile map (gen_pin_map_profiles.py)
 3. Compare with committed versions; if diff -> exit 1 with message.

Intended for CI (invoked via custom CMake target or separate workflow).
"""
from __future__ import annotations
import subprocess, sys, pathlib, shutil, difflib

ROOT = pathlib.Path(__file__).resolve().parent.parent

SCRIPT_EXTRACT = ROOT / 'scripts' / 'extract_pin_map.py'
SCRIPT_PROFILES = ROOT / 'scripts' / 'gen_pin_map_profiles.py'

FILE_EXTRACTED = ROOT / 'scripts' / 'pin_map_extracted.csv'
FILE_PROFILES  = ROOT / 'scripts' / 'pin_map_profiles.csv'

def run(cmd):
    r = subprocess.run(cmd, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if r.returncode != 0:
        print(r.stdout)
        raise SystemExit(f"Command failed: {' '.join(cmd)}")
    return r.stdout

def compare(path: pathlib.Path, new_content: str) -> bool:
    if not path.exists():
        print(f"[FAIL] Missing committed file: {path}")
        return False
    old = path.read_text(encoding='utf-8')
    if old == new_content:
        print(f"[OK] {path.name} up to date")
        return True
    print(f"[FAIL] {path.name} is stale (diff below):")
    diff = difflib.unified_diff(old.splitlines(), new_content.splitlines(), fromfile='committed', tofile='generated', lineterm='')
    for line in diff:
        print(line)
    return False

def main():
    # 1. Extract variant map to temp string
    extract_out = subprocess.check_output([sys.executable, str(SCRIPT_EXTRACT), '--out', '-'], cwd=ROOT, text=True)
    # Modify extract script to support '-' output if needed; fallback: write to tmp file then read.
    # Simpler: just re-run and read file.
    run([sys.executable, str(SCRIPT_EXTRACT), '--out', str(FILE_EXTRACTED)])
    new_extracted = FILE_EXTRACTED.read_text(encoding='utf-8')

    # 2. Profiles map
    run([sys.executable, str(SCRIPT_PROFILES), str(FILE_PROFILES)])
    new_profiles = FILE_PROFILES.read_text(encoding='utf-8')

    ok = True
    ok &= compare(FILE_EXTRACTED, new_extracted)
    ok &= compare(FILE_PROFILES, new_profiles)
    if not ok:
        print("\nPin map verification failed. Regenerate and commit updated CSVs.")
        raise SystemExit(1)
    print("All pin map CSV files verified up to date.")

if __name__ == '__main__':
    main()
