"""Cross-implementation conformance test for BVG.

For each .bvg in testvectors/, compare the JSON dump produced by
each implementation against the reference expected/*.json.

Usage:
    python conformance.py
    python conformance.py --impl bvg-py
    python conformance.py --impl bvg-py,bvg-java
    python conformance.py --testvectors path/to/testvectors
    python conformance.py --verbose
"""
from __future__ import annotations
import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent
DEFAULT_TV = ROOT / "testvectors"
DEFAULT_IMPLS = ["bvg-py", "bvg-java", "bvg-cpp", "bvg-rust", "bvg-wasm"]
IMPL_COMMANDS = {
    "bvg-py":   "bvg",
    "bvg-java": str(ROOT / "bvg-java.cmd"),
    "bvg-cpp":  str(ROOT / "bvg-cpp.cmd"),
    "bvg-rust": str(ROOT / "bvg-rust.cmd"),
    "bvg-wasm": str(ROOT / "bvg-wasm.cmd"),
}

def run_dump(impl: str, bvg_file: pathlib.Path) -> dict:
    """Run `<impl> dump <file>` and parse JSON from stdout."""
    cmd = IMPL_COMMANDS.get(impl, impl)
    args = [cmd, "dump", str(bvg_file)]
    try:
        r = subprocess.run(args, capture_output=True, text=True, timeout=30)
    except FileNotFoundError:
        raise RuntimeError(f"{impl}: command not found ({cmd})")
    except subprocess.TimeoutExpired:
        raise RuntimeError(f"{impl}: timeout")
    if r.returncode != 0:
        raise RuntimeError(f"{impl}: exit {r.returncode}\n{r.stderr.strip()}")
    try:
        return json.loads(r.stdout)
    except json.JSONDecodeError as e:
        raise RuntimeError(f"{impl}: bad JSON: {e}\nstdout: {r.stdout[:200]}")

def dump_equal(a: dict, b: dict) -> tuple[bool, str]:
    if a.get("width") != b.get("width"):
        return False, f"width {a.get('width')} != {b.get('width')}"
    if a.get("height") != b.get("height"):
        return False, f"height {a.get('height')} != {b.get('height')}"
    ca = a.get("chunks", [])
    cb = b.get("chunks", [])
    if len(ca) != len(cb):
        return False, f"chunk count {len(ca)} != {len(cb)}"
    for i, (x, y) in enumerate(zip(ca, cb)):
        if x.get("type") != y.get("type"):
            return False, f"chunk #{i} type {x.get('type')} != {y.get('type')}"
        if x.get("hex") != y.get("hex"):
            ha = x.get("hex", "")
            hb = y.get("hex", "")
            n = min(len(ha), len(hb))
            diff = next(
                (j for j in range(0, n, 2) if ha[j:j+2] != hb[j:j+2]), n
            )
            return False, (
                f"chunk #{i} ({x.get('type')}) hex differs at byte {diff//2}"
            )
    return True, ""


def find_testvectors(tv: pathlib.Path) -> list[pathlib.Path]:
    return sorted(tv.glob("*.bvg"))


def main(argv=None):
    p = argparse.ArgumentParser()
    p.add_argument("--impl", default=",".join(DEFAULT_IMPLS),
                   help="comma-separated implementation commands")
    p.add_argument("--testvectors", default=str(DEFAULT_TV))
    p.add_argument("--reference", default="expected",
                   help="subdir with expected/*.json")
    p.add_argument("--verbose", "-v", action="store_true")
    args = p.parse_args(argv)

    tv = pathlib.Path(args.testvectors).resolve()
    if not tv.exists():
        print(f"testvectors dir not found: {tv}", file=sys.stderr)
        return 2
    exp_dir = tv / args.reference
    if not exp_dir.exists():
        print(f"expected dir not found: {exp_dir}", file=sys.stderr)
        return 2

    impls = [s.strip() for s in args.impl.split(",") if s.strip()]
    files = find_testvectors(tv)
    if not files:
        print(f"no .bvg files in {tv}", file=sys.stderr)
        return 2

    matrix: dict[str, dict[str, tuple[bool, str]]] = {
        impl: {} for impl in impls
    }

    for impl in impls:
        if args.verbose:
            print(f"[{impl}]", file=sys.stderr)
        for bvg_file in files:
            exp_path = exp_dir / (bvg_file.stem + ".json")
            if not exp_path.exists():
                matrix[impl][bvg_file.name] = (False, "no expected json")
                continue
            try:
                expected = json.loads(exp_path.read_text(encoding="utf-8"))
                actual = run_dump(impl, bvg_file)
                ok, msg = dump_equal(expected, actual)
                matrix[impl][bvg_file.name] = (ok, msg)
            except Exception as e:
                matrix[impl][bvg_file.name] = (False, str(e))

    print()
    header = f"{'file':<22}" + "".join(f"{impl:<14}" for impl in impls)
    print(header)
    print("-" * len(header))
    total_ok = 0
    total = 0
    for bvg_file in files:
        row = f"{bvg_file.name:<22}"
        for impl in impls:
            ok, _msg = matrix[impl][bvg_file.name]
            row += f"{'OK' if ok else 'FAIL':<14}"
            total += 1
            if ok:
                total_ok += 1
        print(row)
    print("-" * len(header))
    print(f"{total_ok}/{total} ok")

    any_fail = False
    for impl in impls:
        for name, (ok, msg) in matrix[impl].items():
            if not ok:
                if not any_fail:
                    print("\nfailures:")
                    any_fail = True
                print(f"  {impl} {name}: {msg}")
    return 0 if total_ok == total else 1


if __name__ == "__main__":
    sys.exit(main())