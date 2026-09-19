#!/usr/bin/env python3
"""
Roda o parser (Python ou C) contra o pacote de 50 casos de teste.

Uso:
    python run_tests_parser.py <caminho_para_parser.py> <pasta_casos>
    python run_tests_parser.py <caminho_para_parser.exe> <pasta_casos> --exe

Exemplos (rodando da raiz do repositório):
    python run_tests_parser.py ../parser.py casos
    python run_tests_parser.py ../parser.exe casos --exe
"""

import re
import subprocess
import sys
from pathlib import Path


def normaliza(s: str) -> str:
    # Ignora diferenças de espaço ao redor de "," e "=" — o próprio
    # ast.esperada.txt do pacote é inconsistente nisso.
    s = re.sub(r"\s*,\s*", ",", s.strip())
    s = re.sub(r"\s*=\s*", "=", s)
    return s


def montar_comando(parser_path: str, codigo: Path, eh_exe: bool) -> list:
    if eh_exe:
        return [parser_path, str(codigo)]
    return [sys.executable, parser_path, str(codigo)]


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    parser_path = sys.argv[1]
    casos_dir = Path(sys.argv[2])
    eh_exe = "--exe" in sys.argv[3:]

    if not casos_dir.is_dir():
        print(f"Pasta de casos não encontrada: {casos_dir}", file=sys.stderr)
        sys.exit(1)

    casos = sorted(p for p in casos_dir.iterdir() if p.is_dir())
    total = 0
    passou = 0
    falhas = []

    for caso in casos:
        codigo = caso / "codigo.c"
        if not codigo.exists():
            continue

        m = re.match(r"^(\d+)_", caso.name)
        if not m:
            continue
        numero = int(m.group(1))
        esperado_aceito = numero <= 25

        total += 1
        cmd = montar_comando(parser_path, codigo, eh_exe)
        r = subprocess.run(cmd, capture_output=True, text=True)
        aceito = (r.returncode == 0)

        if aceito != esperado_aceito:
            falhas.append(
                f"[STATUS] {caso.name}: esperado "
                f"{'ACEITO' if esperado_aceito else 'REJEITADO'}, "
                f"obteve {'ACEITO' if aceito else 'REJEITADO'}\n"
                f"    stderr: {r.stderr.strip()}"
            )
            continue

        if esperado_aceito:
            esperado_ast = (caso / "ast.esperada.txt").read_text(encoding="utf-8").strip()
            obtido = r.stdout.strip()
            if normaliza(obtido) != normaliza(esperado_ast):
                falhas.append(
                    f"[AST] {caso.name}\n"
                    f"    esperado: {esperado_ast}\n"
                    f"    obtido:   {obtido}"
                )
                continue

        passou += 1

    print()
    for f in falhas:
        print(f)
        print()

    print(f"Resumo: {passou} OK / {len(falhas)} falharam / {total} total")
    sys.exit(0 if not falhas else 1)


if __name__ == "__main__":
    main()