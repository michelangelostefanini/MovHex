#!/usr/bin/env python3
"""Compare program output against the saved regression fixtures."""
import argparse
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", nargs="?", default="./movhex")
    args = parser.parse_args()
    binary = Path(args.binary).resolve()
    cases = sorted(Path(__file__).parent.glob("*.txt"))
    if not cases:
        print("No test cases found", file=sys.stderr)
        return 1
    failures = 0
    for case in cases:
        expected = Path(str(case) + ".result")
        try:
            result = subprocess.run([str(binary)], input=case.read_bytes(),
                                    capture_output=True, timeout=120)
            passed = (result.returncode == 0 and not result.stderr and
                      result.stdout.splitlines() == expected.read_bytes().splitlines())
            if not passed:
                print(result.stderr.decode(errors="replace"), file=sys.stderr)
        except (OSError, subprocess.TimeoutExpired) as error:
            print(error, file=sys.stderr)
            passed = False
        print(f"{'PASS' if passed else 'FAIL'} {case.name}")
        failures += not passed
    print(f"{len(cases) - failures}/{len(cases)} tests passed")
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
