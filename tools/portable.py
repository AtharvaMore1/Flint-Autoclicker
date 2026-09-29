"""Build one downloadable EXE using Windows' built-in cabinet compression."""

import hashlib
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"
OUTPUT = ROOT / "build" / "portable"


def main() -> None:
    if not (DIST / "FlintAutoClicker.exe").is_file():
        raise SystemExit("Package the Qt app first.")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    files = sorted(path for path in DIST.rglob("*") if path.is_file())
    directives = [
        '.Set CabinetNameTemplate=payload.cab',
        '.Set DiskDirectoryTemplate=.',
        '.Set MaxDiskSize=0',
        '.Set Cabinet=on',
        '.Set Compress=on',
        '.Set CompressionType=LZX',
        '.Set CompressionMemory=21',
        '.Set MaxCabinetSize=0',
        '.Set MaxFolderSize=0',
        '.Set GenerateInf=off',
    ]
    manifest = []
    for path in files:
        relative = path.relative_to(DIST)
        directives.append(f'"{path}" "{relative}"')
        manifest.append(f"{path.stat().st_size}\t{relative.as_posix()}\n")
    (OUTPUT / "payload.ddf").write_text("\n".join(directives), encoding="ascii")
    (OUTPUT / "manifest").write_text("".join(manifest), encoding="utf-8", newline="\n")
    subprocess.run(["makecab.exe", "/V0", "/F", "payload.ddf"], cwd=OUTPUT, check=True)
    cab = OUTPUT / "payload.cab"
    with cab.open("rb") as stream:
        bundle_id = hashlib.file_digest(stream, "sha256").hexdigest()
    resources = (
        f'1 ICON "{(ROOT / "assets/app.ico").as_posix()}"\n'
        f'101 RCDATA "{cab.as_posix()}"\n'
        f'102 RCDATA "{(OUTPUT / "manifest").as_posix()}"\n'
    )
    (OUTPUT / "portable.rc").write_text(resources, encoding="utf-8")
    subprocess.run(["windres", "portable.rc", "-O", "coff", "-o", "portable.res"], cwd=OUTPUT, check=True)
    destination = OUTPUT / "FlintAutoClicker.exe"
    subprocess.run([
        "g++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-municode", "-mwindows", "-static", "-s",
        f'-DBUNDLE_ID="{bundle_id}"', str(ROOT / "tools/portable.cpp"), "portable.res",
        "-lcabinet", "-lshell32", "-lole32", "-luuid", "-o", str(destination),
    ], cwd=OUTPUT, check=True)
    print(f"Portable executable ready: {destination}")


if __name__ == "__main__":
    main()
