#!/usr/bin/env python3
"""Prepare a Jarvis ESP32 firmware release by updating VERSION and BUILD."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$")


def read_text(path: Path) -> str:
    value = path.read_text(encoding="utf-8").strip()
    if not value:
        raise RuntimeError(f"Empty file: {path}")
    return value


def parse_semver(value: str) -> tuple[int, int, int]:
    match = SEMVER_RE.fullmatch(value)
    if not match:
        raise ValueError(f"Expected SemVer MAJOR.MINOR.PATCH, got: {value}")
    return tuple(int(part) for part in match.groups())  # type: ignore[return-value]


def next_version(current: str, bump: str, custom: str | None) -> str:
    major, minor, patch = parse_semver(current)

    if bump == "major":
        candidate = f"{major + 1}.0.0"
    elif bump == "minor":
        candidate = f"{major}.{minor + 1}.0"
    elif bump == "patch":
        candidate = f"{major}.{minor}.{patch + 1}"
    elif bump == "custom":
        if not custom:
            raise ValueError("custom_version is required when bump=custom")
        candidate_tuple = parse_semver(custom)
        if candidate_tuple <= (major, minor, patch):
            raise ValueError(
                f"Custom version must be greater than current version {current}, got: {custom}"
            )
        candidate = custom
    else:
        raise ValueError(f"Unsupported bump type: {bump}")

    return candidate


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bump", choices=("patch", "minor", "major", "custom"), required=True)
    parser.add_argument("--custom-version", default="")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    root = args.root.resolve()
    version_file = root / "VERSION"
    build_file = root / "BUILD"

    current_version = read_text(version_file)
    current_build_text = read_text(build_file)
    if not current_build_text.isdigit():
        raise ValueError(f"BUILD must contain a non-negative integer, got: {current_build_text}")

    release_version = next_version(current_version, args.bump, args.custom_version.strip() or None)
    release_build = int(current_build_text) + 1

    if args.write:
        version_file.write_text(release_version + "\n", encoding="utf-8")
        build_file.write_text(str(release_build) + "\n", encoding="utf-8")

    print(f"version={release_version}")
    print(f"build={release_build}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
