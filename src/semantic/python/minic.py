#!/usr/bin/env python3
"""MiniC command line: lexical/syntax/semantic validation and IR generation."""

import argparse
import json
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_LEXER_DIR = os.path.normpath(os.path.join(_HERE, "..", "..", "lexer"))
if _LEXER_DIR not in sys.path:
    sys.path.insert(0, _LEXER_DIR)

from semantic import AnalisadorSemantico, ErroSintatico, Lexer, P, ParserPos
from ir import GeradorIR


def _reportar_erros_lexer(lexer):
    for line, column, message in lexer.erros:
        print(f"Erro léxico: linha {line}, coluna {column}: {message}", file=sys.stderr)


def _parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fonte", help="arquivo-fonte MiniC")
    parser.add_argument("--ir", action="store_true", help="emite IR de três endereços")
    parser.add_argument("--check", action="store_true", help="valida sem emitir IR")
    parser.add_argument("--tokens", action="store_true", help="exibe tokens")
    parser.add_argument("--ast", action="store_true", help="exibe a AST")
    parser.add_argument("-O0", action="store_true", help="gera IR sem otimizações")
    parser.add_argument(
        "-O1", action="store_true", help="otimização ainda não disponível"
    )
    return parser


def main(argv=None):
    args = _parser().parse_args(argv)
    if args.O1:
        print("Erro: otimização -O1 ainda não está implementada.", file=sys.stderr)
        return 1
    if args.tokens and (args.ast or args.ir or args.check):
        print("Erro: --tokens não pode ser combinado com outras opções.", file=sys.stderr)
        return 1
    if args.ast and (args.ir or args.check):
        print("Erro: --ast não pode ser combinado com --ir ou --check.", file=sys.stderr)
        return 1
    if args.ir and args.check:
        print("Erro: --ir não pode ser combinado com --check.", file=sys.stderr)
        return 1

    try:
        with open(args.fonte, "r", encoding="utf-8") as source_file:
            source = source_file.read()
    except OSError as error:
        print(f"Erro: não foi possível abrir '{args.fonte}': {error}", file=sys.stderr)
        return 1

    lexer = Lexer(source)
    tokens = lexer.analisar()
    if lexer.erros:
        _reportar_erros_lexer(lexer)
        return 2
    if args.tokens:
        for token in tokens:
            print(json.dumps(token.to_dict(), ensure_ascii=False))
        return 0

    try:
        program = ParserPos(tokens).parse_program()
    except ErroSintatico as error:
        print(f"Erro sintático: {error}", file=sys.stderr)
        return 3

    if args.ast:
        print(program.sexpr())
        return 0

    diagnostics = AnalisadorSemantico(source, tokens).analisar(program)
    if diagnostics:
        for diagnostic in diagnostics:
            print(
                f"Erro semântico [{diagnostic.codigo}] — linha "
                f"{diagnostic.linha}, coluna {diagnostic.coluna}: "
                f"{diagnostic.mensagem}",
                file=sys.stderr,
            )
        return 4

    functions = [
        declaration
        for declaration in program.declaracoes
        if isinstance(declaration, P.Function) and declaration.nome == "main"
    ]
    if len(functions) != 1:
        print(
            "Erro semântico: o programa deve definir exatamente uma função main.",
            file=sys.stderr,
        )
        return 4

    if args.check:
        print("Análise concluída: programa aceito.")
        return 0

    print(GeradorIR().gerar(program), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
