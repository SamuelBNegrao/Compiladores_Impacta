#!/usr/bin/env python3
"""SLR(1) expression parser demonstrating inherited and synthesized attributes."""

import argparse
import sys
from dataclasses import dataclass

from lexer_v2 import Lexer
from lr import LRParseError, Production, SLRParser
import parser as ast


class ErroAtributo(Exception):
    pass


@dataclass(frozen=True)
class ExpressaoTipada:
    arvore: ast.No
    tipo: str


def _numero(values):
    token = values[0]
    kind = "float" if token.tipo == "FLOAT_LIT" else "int"
    return ExpressaoTipada(ast.Lit("real" if kind == "float" else "int", token.lexema), kind)


def _identificador(amb):
    def reduce(values):
        token = values[0]
        if token.lexema not in amb:
            raise ErroAtributo(
                f"identificador '{token.lexema}' não declarado no ambiente herdado"
            )
        tipo = amb[token.lexema]
        if tipo not in ("int", "float"):
            raise ErroAtributo(
                f"identificador '{token.lexema}' tem tipo não numérico: {tipo}"
            )
        return ExpressaoTipada(ast.Id(token.lexema), tipo)
    return reduce


def _promover(left, right):
    if left not in ("int", "float") or right not in ("int", "float"):
        raise ErroAtributo(
            f"operandos aritméticos devem ser numéricos; recebidos {left} e {right}"
        )
    return "float" if "float" in (left, right) else "int"


def _binario(values):
    left, operator, right = values
    result_type = _promover(left.tipo, right.tipo)
    return ExpressaoTipada(
        ast.Binary(operator.lexema, left.arvore, right.arvore), result_type
    )


def _modulo(values):
    left, operator, right = values
    if left.tipo != "int" or right.tipo != "int":
        raise ErroAtributo(
            f"operador '%' exige operandos int; recebidos {left.tipo} e {right.tipo}"
        )
    return ExpressaoTipada(
        ast.Binary(operator.lexema, left.arvore, right.arvore), "int"
    )


def _unario(values):
    operator, operand = values
    if operand.tipo not in ("int", "float"):
        raise ErroAtributo(
            f"operador unário exige tipo numérico; recebido {operand.tipo}"
        )
    return ExpressaoTipada(ast.Unary(operator.lexema, operand.arvore), operand.tipo)


def _identity(values):
    return values[0]


def _parenthesized(values):
    return values[1]


def _grammar(amb):
    rules = []

    def add(lhs, rhs, action):
        rules.append(Production(lhs, tuple(rhs.split()) if rhs else (), action))

    add("Expr", "Expr PLUS Term", "binary")
    add("Expr", "Expr MINUS Term", "binary")
    add("Expr", "Term", "identity")
    add("Term", "Term STAR Factor", "binary")
    add("Term", "Term SLASH Factor", "binary")
    add("Term", "Term PERCENT Factor", "modulo")
    add("Term", "Factor", "identity")
    add("Factor", "MINUS Factor", "unary")
    add("Factor", "INT_LIT", "number")
    add("Factor", "FLOAT_LIT", "number")
    add("Factor", "IDENT", "identifier")
    add("Factor", "LPAREN Expr RPAREN", "parenthesized")

    reducers = {
        "binary": _binario,
        "modulo": _modulo,
        "identity": _identity,
        "unary": _unario,
        "number": _numero,
        "identifier": _identificador(amb),
        "parenthesized": _parenthesized,
    }
    return SLRParser("Expr", rules, reducers)


def analisar(expressao, ambiente, trace=None):
    tokens = Lexer(expressao).analisar()
    return _grammar(ambiente).parse(tokens, trace=trace)


def _parse_environment(value):
    environment = {}
    if not value:
        return environment
    for entry in value.split(","):
        name, separator, kind = entry.partition("=")
        if not separator or not name or kind not in ("int", "float"):
            raise ValueError(
                "ambiente inválido; use identificador=int ou identificador=float"
            )
        environment[name] = kind
    return environment


def main():
    command = argparse.ArgumentParser(description=__doc__)
    command.add_argument("expressao", help="expressão aritmética a analisar")
    command.add_argument(
        "--env",
        default="",
        help="ambiente herdado, por exemplo: x=int,y=float",
    )
    command.add_argument("--trace", action="store_true", help="exibe shift/reduce")
    args = command.parse_args()

    try:
        environment = _parse_environment(args.env)
        trace = (lambda event: print(event, file=sys.stderr)) if args.trace else None
        result = analisar(args.expressao, environment, trace)
    except (ValueError, ErroAtributo, LRParseError) as error:
        print(f"Erro: {error}", file=sys.stderr)
        return 1

    print(f"AST: {result.arvore.sexpr()}")
    print(f"tipo sintetizado: {result.tipo}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
