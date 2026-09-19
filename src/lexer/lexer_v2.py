import sys
import json
from dataclasses import dataclass

@dataclass
class Token:
    tipo: str
    lexema: str
    atributo: object
    linha: int
    coluna: int

    def to_dict(self):
        return {
            "token": self.tipo,
            "lexeme": self.lexema,
            "attribute": self.atributo,
            "line": self.linha,
            "column": self.coluna
        }

PALAVRAS_RESERVADAS = {
    "int": "INT", "float": "FLOAT", "bool": "BOOL", "char": "CHAR", "void": "VOID",
    "if": "IF", "else": "ELSE", "while": "WHILE", "for": "FOR",
    "return": "RETURN", "break": "BREAK", "continue": "CONTINUE",
    "true": "TRUE", "false": "FALSE", "print": "PRINT", "read": "READ"
}

OPERADORES = {
    "==": "EQ", "!=": "NE", "<=": "LE", ">=": "GE",
    "&&": "AND", "||": "OR", "+": "PLUS", "-": "MINUS",
    "*": "STAR", "/": "SLASH", "%": "PERCENT", "<": "LT",
    ">": "GT", "!": "NOT", "=": "ASSIGN"
}

DELIMITADORES = {
    "(": "LPAREN", ")": "RPAREN",
    "[": "LBRACKET", "]": "RBRACKET",
    "{": "LBRACE", "}": "RBRACE",
    ";": "SEMICOLON", ",": "COMMA",
    ".": "DOT"
}

class Lexer:
    def __init__(self, codigo):
        self.codigo = codigo
        self.posicao = 0
        self.linha = 1
        self.coluna = 1
        self.tokens = []

    def fim(self):
        return self.posicao >= len(self.codigo)

    def atual(self):
        return "\0" if self.fim() else self.codigo[self.posicao]

    def proximo(self):
        return "\0" if self.posicao + 1 >= len(self.codigo) else self.codigo[self.posicao + 1]

    def avancar(self):
        if self.fim():
            return "\0"
        caractere = self.codigo[self.posicao]
        self.posicao += 1
        if caractere == "\n":
            self.linha += 1
            self.coluna = 1
        else:
            self.coluna += 1
        return caractere

    def adicionar_token(self, tipo, lexema, linha, coluna, atributo=None):
        if atributo is None:
            if tipo == "INT_LIT":
                try:
                    atributo = int(lexema)
                except ValueError:
                    atributo = lexema
            elif tipo == "FLOAT_LIT":
                try:
                    atributo = float(lexema)
                except ValueError:
                    atributo = lexema
            elif tipo in ("CHAR_LIT", "STRING_LIT"):
                if len(lexema) >= 2 and lexema[0] == lexema[-1] and lexema[0] in ('"', "'"):
                    atributo = lexema[1:-1]
                elif len(lexema) >= 1 and lexema[0] in ('"', "'"):
                    atributo = lexema[1:]
                else:
                    atributo = lexema
            elif tipo == "IDENT":
                atributo = lexema
            else:
                atributo = None

        token = Token(
            tipo=tipo,
            lexema=lexema,
            atributo=atributo,
            linha=linha,
            coluna=coluna
        )
        self.tokens.append(token)

    def ler_identificador(self):
        linha_ini, coluna_ini, inicio = self.linha, self.coluna, self.posicao
        self.avancar()
        while self.atual().isalnum() or self.atual() == "_":
            self.avancar()
        lexema = self.codigo[inicio:self.posicao]
        tipo = PALAVRAS_RESERVADAS.get(lexema, "IDENT")
        self.adicionar_token(tipo, lexema, linha_ini, coluna_ini)

    def ler_numero(self):
        linha_ini, coluna_ini, inicio = self.linha, self.coluna, self.posicao
        while self.atual().isdigit():
            self.avancar()
        if self.atual() == "." and self.proximo().isdigit():
            self.avancar()
            while self.atual().isdigit():
                self.avancar()
            tipo = "FLOAT_LIT"
        else:
            tipo = "INT_LIT"
        lexema = self.codigo[inicio:self.posicao]
        self.adicionar_token(tipo, lexema, linha_ini, coluna_ini)

    def ler_caractere(self):
        linha_ini, coluna_ini, inicio = self.linha, self.coluna, self.posicao
        self.avancar()
        
        while not self.fim() and self.atual() != "'" and self.atual() != "\n":
            if self.atual() == "\\":
                self.avancar()
            self.avancar()
            
        if self.atual() == "'":
            self.avancar()
            lexema = self.codigo[inicio:self.posicao]
            self.adicionar_token("CHAR_LIT", lexema, linha_ini, coluna_ini)

    def ler_string(self):
        linha_ini, coluna_ini, inicio = self.linha, self.coluna, self.posicao
        self.avancar()
        
        while not self.fim() and self.atual() != '"' and self.atual() != "\n":
            if self.atual() in (')', ';'):
                break
            if self.atual() == "\\":
                self.avancar()
            self.avancar()
            
        if self.atual() == '"':
            self.avancar()
            lexema = self.codigo[inicio:self.posicao]
            self.adicionar_token("STRING_LIT", lexema, linha_ini, coluna_ini)

    def ignorar_comentario(self):
        if self.atual() == "/" and self.proximo() == "/":
            while not self.fim() and self.atual() != "\n":
                self.avancar()
            return True
        if self.atual() == "/" and self.proximo() == "*":
            self.avancar()
            self.avancar()
            while not self.fim():
                if self.atual() == "*" and self.proximo() == "/":
                    self.avancar()
                    self.avancar()
                    return True
                self.avancar()
        return False

    def ler_operador(self):
        linha_ini, coluna_ini = self.linha, self.coluna
        atual, proximo = self.atual(), self.proximo()
        duplo = atual + proximo
        if duplo in OPERADORES:
            self.avancar()
            self.avancar()
            self.adicionar_token(OPERADORES[duplo], duplo, linha_ini, coluna_ini)
            return
        if atual in OPERADORES:
            self.avancar()
            self.adicionar_token(OPERADORES[atual], atual, linha_ini, coluna_ini)
            return

    def ler_delimitador(self):
        linha_ini, coluna_ini = self.linha, self.coluna
        caractere = self.avancar()
        self.adicionar_token(DELIMITADORES[caractere], caractere, linha_ini, coluna_ini)

    def analisar(self):
        while not self.fim():
            caractere = self.atual()
            if caractere.isspace():
                self.avancar()
                continue
            if caractere == "/" and self.proximo() in ["/", "*"]:
                self.ignorar_comentario()
                continue
            if caractere.isalpha() or caractere == "_":
                self.ler_identificador()
                continue
            if caractere.isdigit():
                self.ler_numero()
                continue
            if caractere == "'":
                self.ler_caractere()
                continue
            if caractere == '"':
                self.ler_string()
                continue
            if caractere in DELIMITADORES:
                self.ler_delimitador()
                continue
            if caractere + self.proximo() in OPERADORES or caractere in OPERADORES:
                self.ler_operador()
                continue
            self.avancar()

        # Token obrigatório EOF no final de cada arquivo
        self.adicionar_token("EOF", "", self.linha, self.coluna, atributo=None)
        return self.tokens

if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(1)

    with open(sys.argv[1], "r", encoding="utf-8") as f:
        codigo = f.read()

    lexer = Lexer(codigo)
    for t in lexer.analisar():
        print(json.dumps(t.to_dict(), ensure_ascii=False))