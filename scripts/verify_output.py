"""Run the compiled simulator and compare its output with the reference sample."""

from pathlib import Path
import subprocess
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: python scripts/verify_output.py PATH_TO_EXECUTABLE", file=sys.stderr)
        return 2

    root = Path(__file__).resolve().parents[1]
    executable = Path(sys.argv[1]).resolve()
    result = subprocess.run(
        [str(executable)], cwd=root, capture_output=True, text=True,
        encoding="utf-8", timeout=30, check=False,
    )
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    if result.returncode != 0:
        print(f"Simulator failed with exit code {result.returncode}.", file=sys.stderr)
        return 1

    expected = (root / "output/Sample_Output.txt").read_text(encoding="utf-8")
    if result.stdout != expected:
        print("Output does not match the reference sample.", file=sys.stderr)
        return 1

    destination = root / "artifacts"
    destination.mkdir(exist_ok=True)
    (destination / "Run_Output.txt").write_text(result.stdout, encoding="utf-8")
    print("\nVerification passed: execution matches the reference sample.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
