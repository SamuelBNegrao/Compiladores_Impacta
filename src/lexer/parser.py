import sys
import os

# Garante que 'lexer_v2' seja encontrado independente do diretório de onde
# o script for chamado, já que parser.py mora na mesma pasta do lexer.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lexer_v2 import Lexer

TIPOS = {"INT", "FLOAT", "BOOL", "CHAR", "VOID"}

# Nomes amigáveis (em português) usados nas mensagens de erro sintático.
NOME_AMIGAVEL = {
    "SEMICOLON": "';'", "COMMA": "','", "LPAREN": "'('", "RPAREN": "')'",
    "LBRACE": "'{'", "RBRACE": "'}'", "LBRACKET": "'['", "RBRACKET": "']'",
    "ASSIGN": "'='", "IDENT": "identificador", "EOF": "fim de arquivo",
}


# ------------------------------------------------------------------
# Nós da AST — cada um sabe se imprimir como S-expressão
# ------------------------------------------------------------------

class No:
    def sexpr(self):
        raise NotImplementedError


class Program(No):
    def __init__(self, declaracoes):
        self.declaracoes = declaracoes

    def sexpr(self):
        return "Program(" + ", ".join(d.sexpr() for d in self.declaracoes) + ")"


class Function(No):
    def __init__(self, tipo, nome, parametros, corpo):
        self.tipo = tipo
        self.nome = nome
        self.parametros = parametros  # lista de (tipo, nome)
        self.corpo = corpo

    def sexpr(self):
        params = ",".join(f"{t} {n}" for t, n in self.parametros)
        return f"Function({self.tipo} {self.nome}({params}) {self.corpo.sexpr()})"


class Block(No):
    def __init__(self, comandos):
        self.comandos = comandos

    def sexpr(self):
        return "Block(" + ", ".join(c.sexpr() for c in self.comandos) + ")"


class VarDecl(No):
    def __init__(self, tipo, nome, tamanho=None, inicializador=None):
        self.tipo = tipo
        self.nome = nome
        self.tamanho = tamanho              # expressão (tamanho do vetor) ou None
        self.inicializador = inicializador  # expressão ou None

    def sexpr(self):
        if self.tamanho is not None:
            return f"VarDecl({self.tipo} {self.nome} size={self.tamanho.sexpr()})"
        if self.inicializador is not None:
            return f"VarDecl({self.tipo} {self.nome}={self.inicializador.sexpr()})"
        return f"VarDecl({self.tipo} {self.nome})"


class If(No):
    def __init__(self, condicao, entao, senao):
        self.condicao = condicao
        self.entao = entao
        self.senao = senao

    def sexpr(self):
        senao = self.senao.sexpr() if self.senao is not None else "NULL"
        return f"If({self.condicao.sexpr()},{self.entao.sexpr()},{senao})"


class While(No):
    def __init__(self, condicao, corpo):
        self.condicao = condicao
        self.corpo = corpo

    def sexpr(self):
        return f"While({self.condicao.sexpr()},{self.corpo.sexpr()})"


class Return(No):
    def __init__(self, expr):
        self.expr = expr

    def sexpr(self):
        return f"Return({self.expr.sexpr() if self.expr is not None else 'NULL'})"


class ExprStmt(No):
    def __init__(self, expr):
        self.expr = expr

    def sexpr(self):
        return f"ExprStmt({self.expr.sexpr()})"


class Assign(No):
    def __init__(self, alvo, valor):
        self.alvo = alvo
        self.valor = valor

    def sexpr(self):
        return f"Assign({self.alvo.sexpr()},{self.valor.sexpr()})"


class Binary(No):
    def __init__(self, op, esq, dire):
        self.op = op
        self.esq = esq
        self.dire = dire

    def sexpr(self):
        return f"Binary({self.op},{self.esq.sexpr()},{self.dire.sexpr()})"


class Unary(No):
    def __init__(self, op, expr):
        self.op = op
        self.expr = expr

    def sexpr(self):
        return f"Unary({self.op},{self.expr.sexpr()})"


class Call(No):
    def __init__(self, callee, args):
        self.callee = callee
        self.args = args

    def sexpr(self):
        partes = [self.callee.sexpr()] + [a.sexpr() for a in self.args]
        return "Call(" + ",".join(partes) + ")"


class Index(No):
    def __init__(self, base, indice):
        self.base = base
        self.indice = indice

    def sexpr(self):
        return f"Index({self.base.sexpr()},{self.indice.sexpr()})"


class Id(No):
    def __init__(self, nome):
        self.nome = nome

    def sexpr(self):
        return f"Id({self.nome})"


class Lit(No):
    def __init__(self, tipo, valor):
        self.tipo = tipo
        self.valor = valor

    def sexpr(self):
        return f"Lit({self.tipo},{self.valor})"


# ------------------------------------------------------------------
# Erro sintático
# ------------------------------------------------------------------

class ErroSintatico(Exception):
    pass


# ------------------------------------------------------------------
# Parser — descida recursiva com 1 token de lookahead
# ------------------------------------------------------------------

class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    # --- utilidades de leitura de tokens -----------------------------

    def atual(self):
        return self.tokens[self.pos]

    def checar(self, tipo):
        return self.atual().tipo == tipo

    def avancar(self):
        tok = self.tokens[self.pos]
        if self.pos < len(self.tokens) - 1:
            self.pos += 1
        return tok

    def casar(self, tipo, descricao=None):
        if self.checar(tipo):
            return self.avancar()
        self.erro(descricao or NOME_AMIGAVEL.get(tipo, tipo))

    def erro(self, esperado):
        tok = self.atual()
        lexema = tok.lexema if tok.lexema else "<fim de arquivo>"
        raise ErroSintatico(
            f"esperado {esperado}, encontrado '{lexema}' ({tok.tipo}) "
            f"na linha {tok.linha}, coluna {tok.coluna}"
        )

    # --- programa / declarações de topo -------------------------------

    def parse_program(self):
        declaracoes = []
        while not self.checar("EOF"):
            declaracoes.append(self.parse_declaracao_topo())
        return Program(declaracoes)

    def parse_tipo(self):
        if self.atual().tipo in TIPOS:
            return self.avancar().lexema
        self.erro("tipo (int, float, bool, char ou void)")

    def parse_declaracao_topo(self):
        if self.atual().tipo in TIPOS:
            tipo = self.parse_tipo()
            nome = self.casar("IDENT", "identificador").lexema
            if self.checar("LPAREN"):
                return self.parse_funcao(tipo, nome)
            return self.parse_resto_var_decl(tipo, nome)
        # comando solto no nível de topo (ex: "a = b = 3;")
        expr = self.parse_expr()
        self.casar("SEMICOLON")
        return ExprStmt(expr)

    def parse_parametro(self):
        tipo = self.parse_tipo()
        nome = self.casar("IDENT", "identificador").lexema
        if self.checar("LBRACKET"):
            self.avancar()
            self.parse_expr()
            self.casar("RBRACKET")
        return (tipo, nome)

    def parse_funcao(self, tipo, nome):
        self.casar("LPAREN")
        parametros = []
        if not self.checar("RPAREN"):
            parametros.append(self.parse_parametro())
            while self.checar("COMMA"):
                self.avancar()
                parametros.append(self.parse_parametro())
        self.casar("RPAREN", "tipo de parâmetro ou ')'")
        corpo = self.parse_block()
        return Function(tipo, nome, parametros, corpo)

    def parse_resto_var_decl(self, tipo, nome):
        tamanho = None
        inicializador = None
        if self.checar("LBRACKET"):
            self.avancar()
            tamanho = self.parse_expr()
            self.casar("RBRACKET")
        if self.checar("ASSIGN"):
            self.avancar()
            inicializador = self.parse_expr()
        self.casar("SEMICOLON")
        return VarDecl(tipo, nome, tamanho, inicializador)

    # --- comandos -------------------------------------------------------

    def parse_block(self):
        self.casar("LBRACE")
        comandos = []
        while not self.checar("RBRACE") and not self.checar("EOF"):
            comandos.append(self.parse_stmt())
        self.casar("RBRACE")
        return Block(comandos)

    def parse_stmt(self):
        if self.checar("LBRACE"):
            return self.parse_block()
        if self.checar("IF"):
            return self.parse_if()
        if self.checar("WHILE"):
            return self.parse_while()
        if self.checar("RETURN"):
            return self.parse_return()
        if self.checar("PRINT"):
            return self.parse_print()
        if self.checar("READ"):
            return self.parse_read()
        if self.atual().tipo in TIPOS:
            tipo = self.parse_tipo()
            nome = self.casar("IDENT", "identificador").lexema
            return self.parse_resto_var_decl(tipo, nome)
        expr = self.parse_expr()
        self.casar("SEMICOLON")
        return ExprStmt(expr)

    def parse_print(self):
        self.casar("PRINT")
        self.casar("LPAREN")
        args = []
        if not self.checar("RPAREN"):
            args.append(self.parse_expr())
            while self.checar("COMMA"):
                self.avancar()
                args.append(self.parse_expr())
        self.casar("RPAREN", "argumento ou ')' ")
        self.casar("SEMICOLON")
        return ExprStmt(Call(Id("print"), args))

    def parse_read(self):
        self.casar("READ")
        self.casar("LPAREN")
        args = []
        if not self.checar("RPAREN"):
            args.append(self.parse_expr())
            while self.checar("COMMA"):
                self.avancar()
                args.append(self.parse_expr())
        self.casar("RPAREN", "argumento ou ')' ")
        self.casar("SEMICOLON")
        return ExprStmt(Call(Id("read"), args))

    def parse_if(self):
        self.casar("IF")
        self.casar("LPAREN")
        condicao = self.parse_expr()
        self.casar("RPAREN")
        entao = self.parse_stmt()
        senao = None
        if self.checar("ELSE"):
            self.avancar()
            senao = self.parse_stmt()
        return If(condicao, entao, senao)

    def parse_while(self):
        self.casar("WHILE")
        self.casar("LPAREN")
        condicao = self.parse_expr()
        self.casar("RPAREN")
        corpo = self.parse_stmt()
        return While(condicao, corpo)

    def parse_return(self):
        self.casar("RETURN")
        if self.checar("SEMICOLON"):
            self.avancar()
            return Return(None)
        expr = self.parse_expr()
        self.casar("SEMICOLON")
        return Return(expr)

    # --- expressões (precedência crescente) ------------------------------
    # expr -> assign
    # assign -> logicOr ('=' assign)?              (associativo à direita)
    # logicOr -> logicAnd ('||' logicAnd)*
    # logicAnd -> equality ('&&' equality)*
    # equality -> relational (('==' | '!=') relational)*
    # relational -> additive (('<'|'>'|'<='|'>=') additive)*
    # additive -> mult (('+'|'-') mult)*
    # mult -> unary (('*'|'/'|'%') unary)*
    # unary -> ('-' | '!') unary | postfix
    # postfix -> primary ( '[' expr ']' | '(' args? ')' )*
    # primary -> IDENT | literais | '(' expr ')'

    def parse_expr(self):
        return self.parse_assign()

    def parse_assign(self):
        esq = self.parse_or()
        if self.checar("ASSIGN"):
            self.avancar()
            valor = self.parse_assign()
            return Assign(esq, valor)
        return esq

    def _bin_esquerda(self, proximo_nivel, operadores):
        esq = proximo_nivel()
        while self.atual().tipo in operadores:
            op = self.avancar().lexema
            dire = proximo_nivel()
            esq = Binary(op, esq, dire)
        return esq

    def parse_or(self):
        return self._bin_esquerda(self.parse_and, {"OR"})

    def parse_and(self):
        return self._bin_esquerda(self.parse_equality, {"AND"})

    def parse_equality(self):
        return self._bin_esquerda(self.parse_relational, {"EQ", "NE"})

    def parse_relational(self):
        return self._bin_esquerda(self.parse_additive, {"LT", "GT", "LE", "GE"})

    def parse_additive(self):
        return self._bin_esquerda(self.parse_mult, {"PLUS", "MINUS"})

    def parse_mult(self):
        return self._bin_esquerda(self.parse_unary, {"STAR", "SLASH", "PERCENT"})

    def parse_unary(self):
        if self.atual().tipo in ("MINUS", "NOT"):
            op = self.avancar().lexema
            expr = self.parse_unary()
            return Unary(op, expr)
        return self.parse_postfix()

    def parse_postfix(self):
        expr = self.parse_primary()
        while True:
            if self.checar("LBRACKET"):
                self.avancar()
                indice = self.parse_expr()
                self.casar("RBRACKET")
                expr = Index(expr, indice)
            elif self.checar("LPAREN"):
                self.avancar()
                args = []
                if not self.checar("RPAREN"):
                    args.append(self.parse_expr())
                    while self.checar("COMMA"):
                        self.avancar()
                        args.append(self.parse_expr())
                self.casar("RPAREN", "argumento ou ')'")
                expr = Call(expr, args)
            else:
                break
        return expr

    def parse_primary(self):
        tok = self.atual()
        if tok.tipo == "INT_LIT":
            self.avancar()
            return Lit("int", tok.lexema)
        if tok.tipo == "FLOAT_LIT":
            self.avancar()
            return Lit("real", tok.lexema)
        if tok.tipo == "CHAR_LIT":
            self.avancar()
            return Lit("char", tok.atributo if tok.atributo is not None else tok.lexema)
        if tok.tipo == "STRING_LIT":
            self.avancar()
            return Lit("string", tok.atributo if tok.atributo is not None else tok.lexema)
        if tok.tipo == "TRUE":
            self.avancar()
            return Lit("bool", "true")
        if tok.tipo == "FALSE":
            self.avancar()
            return Lit("bool", "false")
        if tok.tipo == "IDENT":
            self.avancar()
            return Id(tok.lexema)
        if tok.tipo == "LPAREN":
            self.avancar()
            expr = self.parse_expr()
            self.casar("RPAREN")
            return expr
        self.erro("expressão (identificador, literal, '(' ou operador unário)")


# ------------------------------------------------------------------
# CLI: python parser.py codigo.c
# ------------------------------------------------------------------

def main():
    if len(sys.argv) < 2:
        print("uso: python parser.py <arquivo.c>", file=sys.stderr)
        sys.exit(1)

    caminho = sys.argv[1]
    try:
        with open(caminho, "r", encoding="utf-8") as f:
            codigo = f.read()
    except OSError as e:
        print(f"erro ao abrir arquivo: {e}", file=sys.stderr)
        sys.exit(1)

    tokens = Lexer(codigo).analisar()

    try:
        ast = Parser(tokens).parse_program()
    except ErroSintatico as e:
        print(f"Erro sintático: {e}", file=sys.stderr)
        sys.exit(1)

    print(ast.sexpr())
    sys.exit(0)


if __name__ == "__main__":
    main()
