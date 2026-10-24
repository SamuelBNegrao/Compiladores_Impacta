#!/usr/bin/env python3
"""Build the C MiniC compiler and run the shared IR fixture suite."""

from __future__ import annotations

import re
import subprocess
import sys
import tempfile
from pathlib import Path


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
SOURCE = HERE / "minic.c"
SUITE = REPO / "src" / "semantic" / "python" / "testes-ir" / "testes"
ERROR_EXIT_CODE = re.compile(r"código de saída\s+(\d+)", re.IGNORECASE)


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="minic-ir-c-") as temp_dir:
        compiler = Path(temp_dir) / "minic-ir-c.exe"
        build = subprocess.run(
            [
                "gcc",
                "-Wall",
                "-Wextra",
                "-std=c11",
                str(SOURCE),
                str(HERE / "ir.c"),
                str(HERE / "optimizer.c"),
                "-o",
                str(compiler),
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if build.returncode != 0:
            sys.stderr.write(build.stderr)
            return build.returncode or 1

        passed = 0
        failed = 0
        groups = (
            ("01-validos-sem-otimizacao", 0),
            ("03-otimizacoes", 0),
            ("02-erros", None),
        )
        for group, fixed_code in groups:
            for source in sorted((SUITE / group).glob("*/*.c")):
                expected_code = fixed_code
                if expected_code is None:
                    reference = source.with_suffix(".txt").read_text(encoding="utf-8")
                    match = ERROR_EXIT_CODE.search(reference)
                    if match is None:
                        print(f"[FALHA] {source.parent.name}: código esperado ausente")
                        failed += 1
                        continue
                    expected_code = int(match.group(1))
                result = subprocess.run(
                    [str(compiler), "--ir", str(source)],
                    capture_output=True,
                    text=True,
                    encoding="utf-8",
                    errors="replace",
                    check=False,
                )
                valid_output = expected_code != 0 or (
                    "func main" in result.stdout and "endfunc" in result.stdout
                )
                if result.returncode == expected_code and valid_output:
                    print(f"[OK] {source.parent.name}: código {expected_code}")
                    passed += 1
                else:
                    details = (result.stderr or result.stdout).strip()
                    print(
                        f"[FALHA] {source.parent.name}: código {result.returncode}, "
                        f"esperado {expected_code}. {details}"
                    )
                    failed += 1
        optimization_criteria = {
            "O01": lambda output: "print 20" in output,
            "O02": lambda output: "print 13" in output,
            "O03": lambda output: "print 6" in output,
            "O04": lambda output: "temporario" not in output,
            "O05": lambda output: "print 22" in output and "print 23" in output,
            "O06": lambda output: "print 10" in output,
            "O07": lambda output: "print 11" in output and "print 22" not in output,
            "O08": lambda output: "t2 = 42" in output
            and output.index("t2 = 42") < output.index("L_test"),
            "O09": lambda output: "L_test" in output and "print" in output,
            "O10": lambda output: "print 99" not in output,
        }
        for source in sorted((SUITE / "03-otimizacoes").glob("*/*.c")):
            result = subprocess.run(
                [str(compiler), "-O1", str(source)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                check=False,
            )
            criterion = optimization_criteria[source.stem]
            if (
                result.returncode == 0
                and "func main" in result.stdout
                and criterion(result.stdout)
            ):
                print(f"[OK] {source.stem}: critério O1")
                passed += 1
            else:
                details = (result.stderr or result.stdout).strip()
                print(f"[FALHA] {source.stem}: {details or 'critério O1 não atendido'}")
                failed += 1
        flow_cases = {
            "for_continue.c": (
                "int main() { int i = 0; "
                "for (; i < 3; i = i + 1) { "
                "if (i == 1) continue; print(i); } return 0; }",
                "goto L_increment_",
            ),
            "while_break.c": (
                "int main() { int i = 0; while (i < 3) { "
                "if (i == 1) break; i = i + 1; } return 0; }",
                "goto L_end_",
            ),
        }
        for filename, (source, required_instruction) in flow_cases.items():
            fixture = Path(temp_dir) / filename
            fixture.write_text(source, encoding="utf-8")
            result = subprocess.run(
                [str(compiler), "--ir", str(fixture)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                check=False,
            )
            if result.returncode == 0 and required_instruction in result.stdout:
                print(f"[OK] {filename}: fluxo de controle")
                passed += 1
            else:
                print(f"[FALHA] {filename}: {result.stderr.strip() or result.stdout.strip()}")
                failed += 1
        option_fixture = Path(temp_dir) / "options.c"
        option_fixture.write_text("int main() { return 0; }", encoding="utf-8")
        for option in ("--ir", "-O0", "-O1"):
            result = subprocess.run(
                [str(compiler), "--check", option, str(option_fixture)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                check=False,
            )
            if result.returncode != 0:
                print(f"[OK] --check {option}: combinação rejeitada")
                passed += 1
            else:
                print(f"[FALHA] --check {option}: combinação aceita")
                failed += 1
        logical_fixture = Path(temp_dir) / "logical_constants.c"
        logical_fixture.write_text(
            "int main() { if (true && false) print(1); "
            "else print(2); return 0; }",
            encoding="utf-8",
        )
        logical_result = subprocess.run(
            [str(compiler), "-O1", str(logical_fixture)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if (
            logical_result.returncode == 0
            and "print 2" in logical_result.stdout
            and "print 1" not in logical_result.stdout
        ):
            print("[OK] logical_constants.c: simplificação de expressão lógica")
            passed += 1
        else:
            print(
                "[FALHA] logical_constants.c: "
                f"{logical_result.stderr.strip() or logical_result.stdout}"
            )
            failed += 1
        cse_source = (
            "int main() { int a; int b; int x; int y; read(a, b); "
            "x = a * b + 1; y = a * b + 2; "
            "print(x); print(y); return 0; }"
        )
        cse_fixture = Path(temp_dir) / "common_subexpression.c"
        cse_fixture.write_text(cse_source, encoding="utf-8")
        cse_result = subprocess.run(
            [str(compiler), "-O1", str(cse_fixture)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        if cse_result.returncode == 0 and cse_result.stdout.count(" = a * b\n") == 1:
            print("[OK] common_subexpression.c: eliminação local de subexpressão comum")
            passed += 1
        else:
            print(f"[FALHA] common_subexpression.c: {cse_result.stderr.strip() or cse_result.stdout}")
            failed += 1
        print(f"\nResultado: {passed}/47 verificações aprovadas; {failed} falhas.")
        print("O09 permite não aplicar redução de força, conforme o critério da suíte.")
        return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
