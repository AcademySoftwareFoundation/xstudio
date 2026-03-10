#!/usr/bin/env python3
"""Capture repeatable build and runtime perf baselines as JSON."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from typing import List, Tuple, Union


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", default=".", help="Source directory to measure.")
    parser.add_argument("--build-dir", default="build", help="Build directory to measure.")
    parser.add_argument("--output", required=True, help="Path to the JSON output file.")
    parser.add_argument("--cmake", default="cmake", help="CMake executable to use.")
    parser.add_argument("--ctest", default="ctest", help="CTest executable to use.")
    parser.add_argument(
        "--configure-preset", help="Configure preset to run before collecting results."
    )
    parser.add_argument("--build-preset", help="Build preset to run before collecting results.")
    parser.add_argument("--test-preset", help="CTest preset to run before collecting results.")
    parser.add_argument(
        "--command",
        action="append",
        default=[],
        help="Extra named command to run, formatted as name:command.",
    )
    return parser.parse_args()


def run_command(
    name: str, command: Union[List[str], str], cwd: Path, shell: bool = False
) -> dict:
    start = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=str(cwd),
        shell=shell,
        capture_output=True,
        text=True,
        check=False,
    )
    duration = time.perf_counter() - start

    return {
        "name": name,
        "command": command if isinstance(command, str) else " ".join(command),
        "cwd": str(cwd),
        "duration_seconds": round(duration, 6),
        "returncode": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
    }


def split_named_command(value: str) -> Tuple[str, str]:
    if ":" not in value:
        raise argparse.ArgumentTypeError(
            f"Invalid command '{value}'. Expected the format name:command."
        )
    name, command = value.split(":", 1)
    if not name or not command:
        raise argparse.ArgumentTypeError(
            f"Invalid command '{value}'. Expected the format name:command."
        )
    return name, command


def main() -> int:
    args = parse_args()
    source_dir = Path(args.source_dir).resolve()
    build_dir = Path(args.build_dir).resolve()
    command_dir = build_dir if build_dir.exists() else source_dir
    output_path = Path(args.output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    results: List[dict] = []

    if args.configure_preset:
        results.append(
            run_command(
                "configure",
                [args.cmake, "--preset", args.configure_preset],
                source_dir,
            )
        )

    if args.build_preset:
        results.append(
            run_command(
                "build",
                [args.cmake, "--build", "--preset", args.build_preset],
                source_dir,
            )
        )

    if args.test_preset:
        results.append(
            run_command(
                "test",
                [args.ctest, "--preset", args.test_preset, "--output-on-failure"],
                source_dir,
            )
        )

    for raw_command in args.command:
        name, command = split_named_command(raw_command)
        results.append(run_command(name, command, command_dir, shell=True))

    payload = {
        "source_dir": str(source_dir),
        "build_dir": str(build_dir),
        "timestamp_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "environment": {
            "cmake": args.cmake,
            "ctest": args.ctest,
            "python": sys.executable,
            "qt6_dir": os.environ.get("Qt6_DIR", ""),
            "vcpkg_root": os.environ.get("VCPKG_ROOT", ""),
        },
        "commands": results,
        "summary": {
            "all_passed": all(result["returncode"] == 0 for result in results),
            "total_duration_seconds": round(
                sum(result["duration_seconds"] for result in results), 6
            ),
        },
    }

    output_path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    print(output_path)
    return 0 if payload["summary"]["all_passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
