"""Inject VERSION and BUILD into every PlatformIO firmware build."""

from pathlib import Path

Import("env")  # type: ignore[name-defined]  # PlatformIO/SCons injects this.

project_dir = Path(env.subst("$PROJECT_DIR"))
version_file = project_dir / "VERSION"
build_file = project_dir / "BUILD"


def read_text(path: Path, label: str) -> str:
    if not path.is_file():
        raise RuntimeError(f"{label} file not found: {path}")
    value = path.read_text(encoding="utf-8").strip()
    if not value:
        raise RuntimeError(f"{label} file is empty: {path}")
    return value


version = read_text(version_file, "VERSION")
build_raw = read_text(build_file, "BUILD")

try:
    build = int(build_raw)
except ValueError as exc:
    raise RuntimeError(f"BUILD must contain an integer, got: {build_raw!r}") from exc

if build < 1:
    raise RuntimeError("BUILD must be >= 1")

# Quotes are part of the compiler define value, so C++ receives a string literal.
env.Append(
    CPPDEFINES=[
        ("VOICE_SATELLITE_VERSION", f'\\"{version}\\"'),
        ("VOICE_SATELLITE_BUILD", build),
    ]
)

print(f"Voice Satellite firmware version: {version} Build {build}")
