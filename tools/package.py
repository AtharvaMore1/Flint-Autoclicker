"""Collect the Windows app and Qt Quick runtime for the GitHub Actions artifact."""

from pathlib import Path
import re
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / "build" / "FlintAutoClicker.exe"
OUTPUT = ROOT / "dist"


def main() -> None:
    if not EXE.is_file():
        raise SystemExit("The GitHub workflow must build the app first.")

    deploy = shutil.which("windeployqt6") or shutil.which("windeployqt")
    objdump = shutil.which("objdump")
    if not deploy or not objdump:
        raise SystemExit("Qt deployment tools are missing from the CI environment.")

    if OUTPUT.exists():
        if OUTPUT.resolve().parent != ROOT.resolve() or OUTPUT.name != "dist":
            raise SystemExit("Refusing to replace an unexpected output path.")
        shutil.rmtree(OUTPUT)
    OUTPUT.mkdir()
    shutil.copy2(EXE, OUTPUT / EXE.name)
    shutil.copy2(ROOT / "README.md", OUTPUT / "README.md")

    subprocess.run(
        [deploy, "--qmldir", str(ROOT / "qml"), "--no-translations", "--dir", str(OUTPUT), str(OUTPUT / EXE.name)],
        check=True,
    )

    # MSYS2's Qt deployer does not necessarily include the GCC runtime DLLs.
    bin_dir = Path(deploy).resolve().parent
    scanned: set[Path] = set()
    pending = [path for path in OUTPUT.rglob("*") if path.suffix.lower() in {".dll", ".exe"}]
    while pending:
        binary = pending.pop()
        if binary in scanned:
            continue
        scanned.add(binary)
        result = subprocess.run([objdump, "-p", str(binary)], capture_output=True, text=True, check=True)
        for name in re.findall(r"DLL Name:\s*(\S+)", result.stdout):
            destination = OUTPUT / name
            source = bin_dir / name
            if destination.exists() or not source.is_file():
                continue
            shutil.copy2(source, destination)
            pending.append(destination)

    print(f"Windows artifact ready: {OUTPUT}")


if __name__ == "__main__":
    main()
