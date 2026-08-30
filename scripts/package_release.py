#!/usr/bin/env python3
"""Create versioned board firmware assets for a GitHub Release."""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path


def read_value(path: Path) -> str:
    value = path.read_text(encoding="utf-8").strip()
    if not value:
        raise RuntimeError(f"Empty file: {path}")
    return value


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--environment", default="waveshare-1_85c")
    parser.add_argument("--board", default="waveshare-esp32-s3-touch-lcd-1.85c")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    version = read_value(root / "VERSION")
    build = read_value(root / "BUILD")
    build_dir = root / ".pio" / "build" / args.environment
    dist = root / "dist"
    dist.mkdir(exist_ok=True)

    required = {
        "bootloader.bin": build_dir / "bootloader.bin",
        "partitions.bin": build_dir / "partitions.bin",
        "firmware.bin": build_dir / "firmware.bin",
    }
    missing = [str(path) for path in required.values() if not path.is_file()]
    if missing:
        raise RuntimeError("Missing PlatformIO build output:\n- " + "\n- ".join(missing))

    stem = f"voice-satellite-esp32-{args.board}-v{version}-build{build}"
    package_dir = dist / stem
    package_dir.mkdir(parents=True)

    bootloader = package_dir / "bootloader.bin"
    partitions = package_dir / "partitions.bin"
    app = package_dir / f"{stem}-app.bin"
    merged = package_dir / f"{stem}-factory.bin"

    shutil.copy2(required["bootloader.bin"], bootloader)
    shutil.copy2(required["partitions.bin"], partitions)
    shutil.copy2(required["firmware.bin"], app)

    # ESP32-S3 default flash layout: bootloader 0x0, partition table 0x8000,
    # application/OTA_0 0x10000. The repository partitions.csv also starts app0 at 0x10000.
    subprocess.run(
        [
            sys.executable,
            "-m",
            "esptool",
            "--chip",
            "esp32s3",
            "merge-bin",
            "-o",
            str(merged),
            "--flash-size",
            "16MB",
            "0x0",
            str(bootloader),
            "0x8000",
            str(partitions),
            "0x10000",
            str(app),
        ],
        check=True,
    )

    manifest = {
        "project": "voice-satellite-esp32",
        "board": args.board,
        "platformio_environment": args.environment,
        "chip": "esp32s3",
        "version": version,
        "build": int(build),
        "flash_size": "16MB",
        "artifacts": {
            app.name: {"type": "application", "offset": "0x10000", "sha256": sha256(app)},
            merged.name: {"type": "factory", "offset": "0x0", "sha256": sha256(merged)},
            bootloader.name: {"type": "bootloader", "offset": "0x0", "sha256": sha256(bootloader)},
            partitions.name: {"type": "partition_table", "offset": "0x8000", "sha256": sha256(partitions)},
        },
    }
    (package_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )

    flashing_text = (
        f"Voice Satellite - {args.board} firmware\n"
        f"Version {version} Build {build}\n\n"
        "Factory image (new/fully erased board):\n"
        f"  esptool --chip esp32s3 write-flash 0x0 {merged.name}\n\n"
        "Application image only:\n"
        f"  esptool --chip esp32s3 write-flash 0x10000 {app.name}\n\n"
        "The factory image contains bootloader, partition table and application.\n"
        "The application image is intended for application-only updates / future OTA handling.\n"
    )
    (package_dir / "FLASHING.txt").write_text(flashing_text, encoding="utf-8")

    zip_path = dist / f"{stem}.zip"
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(package_dir.iterdir()):
            archive.write(path, arcname=f"{stem}/{path.name}")

    # Also expose the two most useful binaries directly as GitHub release assets.
    shutil.copy2(app, dist / app.name)
    shutil.copy2(merged, dist / merged.name)

    checksums = dist / f"{stem}-SHA256SUMS.txt"
    downloadable = [dist / app.name, dist / merged.name, zip_path]
    checksums.write_text(
        "".join(f"{sha256(path)}  {path.name}\n" for path in downloadable),
        encoding="utf-8",
    )

    # Only downloadable files should remain in dist/. The expanded directory is
    # already contained in the ZIP and would otherwise be passed to gh release.
    shutil.rmtree(package_dir)

    print(f"Created release package: {zip_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
