#!/usr/bin/env python3
import subprocess
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parents[3]
TESTS = REPO / 'src' / 'tests' / 'semantic'
BIN = REPO / 'src' / 'semantic' / 'c' / 'minic_test.exe'


def main() -> int:
    if not BIN.exists():
        subprocess.run([
            'gcc', '-Wall', '-Wextra', '-std=c11',
            str(REPO / 'src' / 'semantic' / 'c' / 'minic.c'),
            '-o', str(BIN)
        ], check=True)

    ok = True
    for src in sorted(TESTS.glob('*.c')):
        expected = (TESTS / f'{src.stem}.gabarito').read_text(encoding='utf-8')
        proc = subprocess.run(
            [str(BIN), str(src)],
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace',
        )
        actual = (proc.stdout or '') + (proc.stderr or '')
        if actual.strip() != expected.strip():
            print(f'FALHOU: {src.name}')
            print('--- EXPECTED ---')
            print(expected)
            print('--- ACTUAL ---')
            print(actual)
            ok = False
        else:
            print(f'OK: {src.name}')

    return 0 if ok else 1


if __name__ == '__main__':
    raise SystemExit(main())
