#!/usr/bin/env python3
"""Run the repository's MiniC IR fixtures against the integrated compiler."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent
COMPILER = HERE / "minic.py"
SUITE = HERE / "testes-ir" / "testes"
ERROR_EXIT_CODE = re.compile(r"código de saída\s+(\d+)", re.IGNORECASE)


def run_case(source: Path, expected_exit: int) -> tuple[bool, str]:
    result = subprocess.run(
        [sys.executable, str(COMPILER), str(source), "-O0"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if result.returncode != expected_exit:
        detail = (result.stderr or result.stdout).strip()
        return False, f"código {result.returncode}, esperado {expected_exit}: {detail}"
    if expected_exit == 0 and (
        "func main" not in result.stdout or "endfunc" not in result.stdout
    ):
        return False, "compilou sem emitir uma função main completa"
    return True, ""


def run_valid_cases(directory: Path) -> tuple[int, int]:
    passed = 0
    failed = 0
    for source in sorted(directory.glob("*/*.c")):
        ok, detail = run_case(source, 0)
        if ok:
            print(f"[OK] {source.parent.name}: aceito com IR -O0")
            passed += 1
        else:
            print(f"[FALHA] {source.parent.name}: {detail}")
            failed += 1
    return passed, failed


def run_error_cases(directory: Path) -> tuple[int, int]:
    passed = 0
    failed = 0
    for source in sorted(directory.glob("*/*.c")):
        reference = source.with_suffix(".txt").read_text(encoding="utf-8")
        match = ERROR_EXIT_CODE.search(reference)
        if match is None:
            print(f"[FALHA] {source.parent.name}: referência sem código esperado")
            failed += 1
            continue
        expected_exit = int(match.group(1))
        ok, detail = run_case(source, expected_exit)
        if ok:
            print(f"[OK] {source.parent.name}: rejeitado com código {expected_exit}")
            passed += 1
        else:
            print(f"[FALHA] {source.parent.name}: {detail}")
            failed += 1
    return passed, failed


def run_optimization_cases(directory: Path) -> tuple[int, int]:
    criteria = {
        "O01": lambda output: "print 20" in output,
        "O02": lambda output: "print 13" in output,
        "O03": lambda output: "print 6" in output,
        "O04": lambda output: "temporario" not in output,
        "O05": lambda output: "print 22" in output and "print 23" in output,
        "O06": lambda output: "print 10" in output,
        "O07": lambda output: "print 11" in output and "print 22" not in output,
        "O08": lambda output: "t2 = 42" in output
        and output.index("t2 = 42") < output.index("L_test:"),
        "O09": lambda output: "L_test:" in output and "print" in output,
        "O10": lambda output: "print 99" not in output,
    }
    passed = 0
    failed = 0
    for source in sorted(directory.glob("*/*.c")):
        result = subprocess.run(
            [sys.executable, str(COMPILER), str(source), "-O1"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        criterion = criteria.get(source.stem)
        valid = (
            result.returncode == 0
            and "func main" in result.stdout
            and "endfunc" in result.stdout
            and criterion is not None
            and criterion(result.stdout)
        )
        if valid:
            print(f"[OK] {source.stem}: critério de otimização")
            passed += 1
        else:
            detail = (result.stderr or result.stdout).strip()
            print(f"[FALHA] {source.stem}: {detail or 'critério não atendido'}")
            failed += 1
    return passed, failed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--suite",
        type=Path,
        default=SUITE,
        help="diretório testes da suíte de IR",
    )
    args = parser.parse_args()
    if not args.suite.is_dir():
        parser.error(f"diretório da suíte não encontrado: {args.suite}")

    valid_passed, valid_failed = run_valid_cases(
        args.suite / "01-validos-sem-otimizacao"
    )
    optimization_o0_passed, optimization_o0_failed = run_valid_cases(
        args.suite / "03-otimizacoes"
    )
    optimization_passed, optimization_failed = run_optimization_cases(
        args.suite / "03-otimizacoes"
    )
    errors_passed, errors_failed = run_error_cases(args.suite / "02-erros")

    total_passed = valid_passed + optimization_o0_passed + optimization_passed + errors_passed
    total_failed = valid_failed + optimization_o0_failed + optimization_failed + errors_failed
    print(
        "\nResultado: "
        f"{total_passed}/40 verificações aprovadas; {total_failed} falhas."
    )
    print(
        "O09 aceita qualquer IR -O1 válida, pois redução de força é opcional."
    )
    return 1 if total_failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
