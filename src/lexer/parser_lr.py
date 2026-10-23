#!/usr/bin/env python3
"""SLR(1) shift/reduce parser for the MiniC subset used by the project."""

import argparse
import sys

from lexer_v2 import Lexer
from lr import Production, SLRParser, LRParseError
import parser as ast


def _type(values):
    return values[0].lexema


def _program(values):
    return ast.Program(values[0])


def _declarations_empty(_values):
    return []


def _declarations_append(values):
    return values[0] + [values[1]]


def _declaration(values):
    type_name, name, tail = values
    if tail[0] == "function":
        return ast.Function(type_name, name.lexema, tail[1], tail[2])
    size, initializer = tail[1]
    return ast.VarDecl(type_name, name.lexema, size, initializer)


def _function_tail(values):
    return ("function", values[1], values[3])


def _variable_tail(values):
    return ("variable", values[0])


def _array(values):
    return values[1]


def _none(_values):
    return None


def _initializer(values):
    return values[1]


def _parameters_append(values):
    return values[0] + [values[2]]


def _parameter(values):
    return (values[0], values[1].lexema)


def _block(values):
    return ast.Block(values[1])


def _if_with_else(values):
    return ast.If(values[2], values[4], values[5])


def _while(values):
    return ast.While(values[2], values[4])


def _return(values):
    return ast.Return(values[1])


def _print(values):
    return ast.ExprStmt(ast.Call(ast.Id("print"), values[2]))


def _read(values):
    return ast.ExprStmt(ast.Call(ast.Id("read"), values[2]))


def _variable_statement(values):
    size, initializer = values[2]
    return ast.VarDecl(values[0], values[1].lexema, size, initializer)


def _expression_statement(values):
    return ast.ExprStmt(values[0])


def _assign(values):
    return ast.Assign(values[0], values[2])


def _binary(values):
    return ast.Binary(values[1].lexema, values[0], values[2])


def _unary(values):
    return ast.Unary(values[0].lexema, values[1])


def _index(values):
    return ast.Index(values[0], values[2])


def _call(values):
    return ast.Call(values[0], values[2])


def _arguments_append(values):
    return values[0] + [values[2]]


def _identifier(values):
    return ast.Id(values[0].lexema)


def _literal(values):
    token = values[0]
    kinds = {
        "INT_LIT": "int",
        "FLOAT_LIT": "real",
        "CHAR_LIT": "char",
        "STRING_LIT": "string",
        "TRUE": "bool",
        "FALSE": "bool",
    }
    value = token.lexema
    if token.tipo in ("CHAR_LIT", "STRING_LIT"):
        value = token.atributo if token.atributo is not None else token.lexema
    return ast.Lit(kinds[token.tipo], value)


def _parenthesized(values):
    return values[1]


def _productions():
    rules = []

    def add(lhs, rhs, action):
        rules.append(Production(lhs, tuple(rhs.split()) if rhs else (), action))

    add("Program", "Declarations", "program")
    add("Declarations", "Declarations Declaration", "declarations_append")
    add("Declarations", "", "declarations_empty")
    add("Declaration", "Type IDENT TopTail", "declaration")
    add("Declaration", "Expr SEMICOLON", "expression_statement")
    add("TopTail", "LPAREN ParametersOpt RPAREN Block", "function_tail")
    add("TopTail", "VarRest SEMICOLON", "variable_tail")
    add("Type", "INT", "type")
    add("Type", "FLOAT", "type")
    add("Type", "BOOL", "type")
    add("Type", "CHAR", "type")
    add("Type", "VOID", "type")
    add("VarRest", "ArrayOpt InitializerOpt", "pair")
    add("ArrayOpt", "LBRACKET Expr RBRACKET", "array")
    add("ArrayOpt", "", "none")
    add("InitializerOpt", "ASSIGN Expr", "initializer")
    add("InitializerOpt", "", "none")
    add("ParametersOpt", "Parameters", "identity")
    add("ParametersOpt", "", "empty_list")
    add("Parameters", "Parameters COMMA Parameter", "parameters_append")
    add("Parameters", "Parameter", "singleton")
    add("Parameter", "Type IDENT ParameterArrayOpt", "parameter")
    add("ParameterArrayOpt", "LBRACKET OptionalExpr RBRACKET", "ignore")
    add("ParameterArrayOpt", "", "none")
    add("OptionalExpr", "Expr", "identity")
    add("OptionalExpr", "", "none")
    add("Block", "LBRACE Statements RBRACE", "block")
    add("Statements", "Statements Statement", "declarations_append")
    add("Statements", "", "declarations_empty")
    add("Statement", "Block", "identity")
    add("Statement", "IF LPAREN Expr RPAREN Statement ElseOpt", "if_with_else")
    add("Statement", "WHILE LPAREN Expr RPAREN Statement", "while")
    add("Statement", "RETURN ReturnOpt SEMICOLON", "return")
    add("Statement", "PRINT LPAREN ArgumentsOpt RPAREN SEMICOLON", "print")
    add("Statement", "READ LPAREN ArgumentsOpt RPAREN SEMICOLON", "read")
    add("Statement", "Type IDENT VarRest SEMICOLON", "variable_statement")
    add("Statement", "Expr SEMICOLON", "expression_statement")
    add("ElseOpt", "ELSE Statement", "identity_second")
    add("ElseOpt", "", "none")
    add("ReturnOpt", "Expr", "identity")
    add("ReturnOpt", "", "none")
    add("ArgumentsOpt", "Arguments", "identity")
    add("ArgumentsOpt", "", "empty_list")
    add("Arguments", "Arguments COMMA Expr", "arguments_append")
    add("Arguments", "Expr", "singleton")
    add("Expr", "Assignment", "identity")
    add("Assignment", "Or ASSIGN Assignment", "assign")
    add("Assignment", "Or", "identity")
    add("Or", "Or OR And", "binary")
    add("Or", "And", "identity")
    add("And", "And AND Equality", "binary")
    add("And", "Equality", "identity")
    add("Equality", "Equality EQ Relational", "binary")
    add("Equality", "Equality NE Relational", "binary")
    add("Equality", "Relational", "identity")
    add("Relational", "Relational LT Additive", "binary")
    add("Relational", "Relational GT Additive", "binary")
    add("Relational", "Relational LE Additive", "binary")
    add("Relational", "Relational GE Additive", "binary")
    add("Relational", "Additive", "identity")
    add("Additive", "Additive PLUS Multiplicative", "binary")
    add("Additive", "Additive MINUS Multiplicative", "binary")
    add("Additive", "Multiplicative", "identity")
    add("Multiplicative", "Multiplicative STAR Unary", "binary")
    add("Multiplicative", "Multiplicative SLASH Unary", "binary")
    add("Multiplicative", "Multiplicative PERCENT Unary", "binary")
    add("Multiplicative", "Unary", "identity")
    add("Unary", "MINUS Unary", "unary")
    add("Unary", "NOT Unary", "unary")
    add("Unary", "Postfix", "identity")
    add("Postfix", "Postfix LBRACKET Expr RBRACKET", "index")
    add("Postfix", "Postfix LPAREN ArgumentsOpt RPAREN", "call")
    add("Postfix", "Primary", "identity")
    add("Primary", "IDENT", "identifier")
    add("Primary", "INT_LIT", "literal")
    add("Primary", "FLOAT_LIT", "literal")
    add("Primary", "CHAR_LIT", "literal")
    add("Primary", "STRING_LIT", "literal")
    add("Primary", "TRUE", "literal")
    add("Primary", "FALSE", "literal")
    add("Primary", "LPAREN Expr RPAREN", "parenthesized")
    return rules


def _pair(values):
    return (values[0], values[1])


def _identity(values):
    return values[0]


def _empty_list(_values):
    return []


def _singleton(values):
    return [values[0]]


def _ignore(_values):
    return None


def _identity_second(values):
    return values[1]


_REDUCERS = {
    "program": _program,
    "declarations_empty": _declarations_empty,
    "declarations_append": _declarations_append,
    "declaration": _declaration,
    "function_tail": _function_tail,
    "variable_tail": _variable_tail,
    "type": _type,
    "pair": _pair,
    "array": _array,
    "none": _none,
    "initializer": _initializer,
    "identity": _identity,
    "empty_list": _empty_list,
    "parameters_append": _parameters_append,
    "singleton": _singleton,
    "parameter": _parameter,
    "ignore": _ignore,
    "block": _block,
    "if_with_else": _if_with_else,
    "while": _while,
    "return": _return,
    "print": _print,
    "read": _read,
    "variable_statement": _variable_statement,
    "expression_statement": _expression_statement,
    "identity_second": _identity_second,
    "arguments_append": _arguments_append,
    "assign": _assign,
    "binary": _binary,
    "unary": _unary,
    "index": _index,
    "call": _call,
    "identifier": _identifier,
    "literal": _literal,
    "parenthesized": _parenthesized,
}


def _parser():
    return SLRParser("Program", _productions(), _REDUCERS)


class ParserLR:
    def __init__(self, tokens):
        self.tokens = tokens

    def parse_program(self, trace=None):
        return _parser().parse(self.tokens, trace=trace)


def main():
    command = argparse.ArgumentParser(description=__doc__)
    command.add_argument("arquivo")
    command.add_argument("--trace", action="store_true", help="exibe ações shift/reduce")
    args = command.parse_args()

    try:
        with open(args.arquivo, encoding="utf-8") as source_file:
            tokens = Lexer(source_file.read()).analisar()
    except OSError as error:
        print(f"erro ao abrir arquivo: {error}", file=sys.stderr)
        return 1

    trace = (lambda event: print(event, file=sys.stderr)) if args.trace else None
    try:
        tree = ParserLR(tokens).parse_program(trace=trace)
    except LRParseError as error:
        print(f"Erro sintático: {error}", file=sys.stderr)
        return 1

    print(tree.sexpr())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
