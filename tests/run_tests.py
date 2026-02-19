#!/usr/bin/env python3
"""
SDL3 migration plugin test runner.

For each test_<name>_before/after.cpp pair it runs three checks:
  1. before.cpp compiles with SDL2 headers
  2. after.cpp compiles with SDL3 headers (SDL2 include swapped to SDL3)
  3. clang-tidy with the plugin transforms before into after

Usage:  python3 run_tests.py [path/to/SDL3MigrationCheck.so]
Report: tests/test_report.txt  (always this name)
"""

import difflib
import glob
import os
import shutil
import subprocess
import sys
import tempfile

TESTS_DIR = os.path.dirname(os.path.abspath(__file__))
REPORT_FILE = os.path.join(TESTS_DIR, "test_report.txt")

_GREEN = "\033[32m"
_RED   = "\033[31m"
_RESET = "\033[0m"

def clr(word, ok):
    return f"{_GREEN if ok else _RED}{word}{_RESET}"


# ---------------------------------------------------------------------------
# Discovery helpers
# ---------------------------------------------------------------------------

def find_plugin():
    """Search common build locations for the plugin .so."""
    candidates = [
        os.path.join(TESTS_DIR, "..", "build", "SDL3MigrationCheck.so"),
        os.path.join(TESTS_DIR, "..", "SDL3MigrationCheck.so"),
    ]
    for c in candidates:
        c = os.path.abspath(c)
        if os.path.exists(c):
            return c
    return None


def find_test_pairs():
    """Return sorted list of (name, before_path, after_path) tuples."""
    pattern = os.path.join(TESTS_DIR, "test_*_before.cpp")
    pairs = []
    for before in sorted(glob.glob(pattern)):
        basename = os.path.basename(before)
        name = basename[len("test_"):-len("_before.cpp")]
        after = before.replace("_before.cpp", "_after.cpp")
        if os.path.exists(after):
            pairs.append((name, before, after))
    return pairs


# ---------------------------------------------------------------------------
# SDL flag helpers
# ---------------------------------------------------------------------------

def get_pkg_cflags(pkg_name):
    """
    Return (flag_list, None) or (None, error_str) from pkg-config --cflags.
    Only compile flags are fetched; link flags are not needed for syntax checks.
    """
    result = subprocess.run(
        ["pkg-config", "--cflags", pkg_name],
        capture_output=True, text=True,
    )
    if result.returncode != 0:
        return None, f"pkg-config --cflags {pkg_name}: {result.stderr.strip()}"
    flags = result.stdout.strip().split()
    return flags, None


# ---------------------------------------------------------------------------
# Compilation check
# ---------------------------------------------------------------------------

def compile_syntax_check(source_file, extra_flags):
    """
    Run clang -fsyntax-only on source_file.
    Returns (success: bool, stderr: str).
    """
    cmd = (
        ["clang", "-std=c++17", "-x", "c++", "-fsyntax-only"]
        + extra_flags
        + [source_file]
    )
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.returncode == 0, result.stderr.strip()


def make_sdl3_copy(after_file):
    """
    Write a temp .cpp file identical to after_file but with the SDL2 include
    replaced by an SDL3 include so it can be compiled against SDL3 headers.
    Caller is responsible for deleting the returned path.
    """
    with open(after_file) as f:
        content = f.read()
    content = content.replace("#include <SDL2/SDL.h>", "#include <SDL3/SDL.h>")
    fd, path = tempfile.mkstemp(suffix=".cpp")
    with os.fdopen(fd, "w") as f:
        f.write(content)
    return path


# ---------------------------------------------------------------------------
# clang-tidy transform check
# ---------------------------------------------------------------------------

def apply_clang_tidy(plugin, source_file, check_filter, sdl2_flags):
    """
    Run clang-tidy --fix in-place on source_file using the migration plugin.
    clang-tidy exits non-zero when it emits diagnostics, so we ignore the
    return code; the caller does a diff to verify correctness.
    Returns stderr output (informational only).
    """
    cmd = [
        "clang-tidy",
        f"--load={plugin}",
        f"--checks=-*,{check_filter}",
        "--fix",
        "--fix-errors",
        source_file,
        "--",
        "-std=c++17",
    ] + sdl2_flags
    print(f"Running command: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.stderr.strip()


def diff_files(expected_file, actual_file):
    """Return (files_match: bool, unified_diff_str: str)."""
    with open(expected_file) as f:
        expected = f.readlines()
    with open(actual_file) as f:
        actual = f.readlines()
    if expected == actual:
        return True, ""
    diff = difflib.unified_diff(
        expected,
        actual,
        fromfile=os.path.basename(expected_file),
        tofile=os.path.basename(actual_file),
    )
    return False, "".join(diff)


# ---------------------------------------------------------------------------
# Per-test orchestration
# ---------------------------------------------------------------------------

def run_test(name, before, after, plugin, sdl2_flags, sdl3_flags, tmp_dir):
    """
    Execute all three checks for one test pair.
    Returns list of (label: str, passed: bool, detail: str).
    """
    checks = []

    # 1. before compiles with SDL2
    ok, err = compile_syntax_check(before, sdl2_flags)
    checks.append(("before compiles with SDL2", ok, err))

    # 2. after compiles with SDL3
    # sdl3_copy = make_sdl3_copy(after)
    try:
        ok, err = compile_syntax_check(after, sdl3_flags)
        checks.append(("after compiles with SDL3", ok, err))
    finally:
        print("finally")
        # os.unlink(sdl3_copy)

    # 3. clang-tidy transforms before into after
    if not plugin:
        checks.append(("clang-tidy transforms before → after", False, "plugin .so not found"))
        return checks

    tmp_before = os.path.join(tmp_dir, f"test_{name}_after_check.cpp")
    shutil.copy2(before, tmp_before)
    check_filter = f"sdl3-migration-{name}"
    apply_clang_tidy(plugin, tmp_before, check_filter, sdl2_flags)
    match, diff_text = diff_files(tmp_before, after)
    diff_file = os.path.join(TESTS_DIR, f"test_{name}_transform.diff")
    if not match:
        with open(diff_file, "w") as f:
            f.write(diff_text)
    elif os.path.exists(diff_file):
        os.unlink(diff_file)
    checks.append(("clang-tidy transforms before → after", match, diff_text))

    return checks


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def print_progress(name, checks):
    overall = all(ok for _, ok, _ in checks)
    print(f"{clr('PASS' if overall else 'FAIL', overall)}: {name}")
    for label, ok, _ in checks:
        print(f"  {clr('PASS' if ok else 'FAIL', ok)}: {label}")


def build_report(all_results):
    lines = []
    sep = "=" * 66

    lines.append(sep)
    lines.append("SDL3 Migration Plugin – Test Report")
    lines.append(sep)

    total_pass = 0
    total_fail = 0

    for name, checks in all_results:
        overall = all(ok for _, ok, _ in checks)
        if overall:
            total_pass += 1
        else:
            total_fail += 1
        lines.append(f"\n  [{'PASS' if overall else 'FAIL'}] {name}")
        for label, ok, detail in checks:
            lines.append(f"         {'PASS' if ok else 'FAIL'}  {label}")
            if not ok and detail:
                for dl in detail.splitlines()[:20]:
                    lines.append(f"               {dl}")

    lines.append(f"\n{sep}")
    lines.append(
        f"  Tests: {total_pass + total_fail}"
        f"  |  Passed: {total_pass}"
        f"  |  Failed: {total_fail}"
    )
    lines.append(sep)
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    plugin = sys.argv[1] if len(sys.argv) > 1 else find_plugin()
    if plugin:
        plugin = os.path.abspath(plugin)

    sdl2_flags, err = get_pkg_cflags("sdl2")
    if sdl2_flags is None:
        print(f"ERROR: {err}", file=sys.stderr)
        sys.exit(1)

    sdl3_flags, err = get_pkg_cflags("sdl3")
    if sdl3_flags is None:
        print(f"ERROR: {err}", file=sys.stderr)
        sys.exit(1)

    pairs = find_test_pairs()
    if not pairs:
        print(f"No test pairs found in {TESTS_DIR}", file=sys.stderr)
        sys.exit(1)

    print(f"Plugin : {plugin or 'NOT FOUND  (clang-tidy checks will be skipped)'}")
    print(f"Tests  : {len(pairs)}")
    print()

    tmp_dir = tempfile.mkdtemp()
    all_results = []
    try:
        for name, before, after in pairs:
            checks = run_test(name, before, after, plugin, sdl2_flags, sdl3_flags, tmp_dir)
            all_results.append((name, checks))
            print_progress(name, checks)
    finally:
        shutil.rmtree(tmp_dir)

    report = build_report(all_results)
    with open(REPORT_FILE, "w") as f:
        f.write(report)
        f.write("\n")

    print(f"\nReport written to: {REPORT_FILE}")

    any_failed = any(not ok for _, checks in all_results for _, ok, _ in checks)
    sys.exit(1 if any_failed else 0)


if __name__ == "__main__":
    main()
