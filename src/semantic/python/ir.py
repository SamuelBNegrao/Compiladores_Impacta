"""Three-address intermediate representation for the MiniC AST."""

from __future__ import annotations

import sys

ast = sys.modules.get("minic_parser")
if ast is None:
    import parser as ast


class GeradorIR:
    def __init__(self):
        self.linhas: list[str] = []
        self.temporario = 0
        self.rotulos: dict[str, int] = {}
        self.escopos: list[dict[str, str]] = []
        self.loop_targets: list[tuple[str, str]] = []
        self.bindings: dict[int, str] = {}
        self.shadowed: dict[str, set[str]] = {}
        self.globais: dict[str, str] = {}

    def gerar(self, programa: ast.Program) -> str:
        self.linhas = []
        self.globais = {}
        self._gerar_programa(programa)
        return "\n".join(self.linhas) + ("\n" if self.linhas else "")

    def _gerar_programa(self, programa):
        self._descobrir_sombreamentos(programa)
        for declaracao in programa.declaracoes:
            if isinstance(declaracao, ast.Function):
                self._gerar_funcao(declaracao)
            elif isinstance(declaracao, ast.VarDecl):
                self._gerar_global(declaracao)

    def _descobrir_sombreamentos(self, programa):
        self.bindings = {}
        self.shadowed = {}
        global_names = {
            declaration.nome
            for declaration in programa.declaracoes
            if isinstance(declaration, ast.VarDecl)
        }
        for function in (
            declaration
            for declaration in programa.declaracoes
            if isinstance(declaration, ast.Function)
        ):
            names = set()
            repeated = set()

            def visit(statement):
                if isinstance(statement, ast.VarDecl):
                    if statement.nome in names:
                        repeated.add(statement.nome)
                    names.add(statement.nome)
                elif isinstance(statement, ast.Block):
                    for child in statement.comandos:
                        visit(child)
                elif isinstance(statement, ast.If):
                    visit(statement.entao)
                    if statement.senao is not None:
                        visit(statement.senao)
                elif isinstance(statement, (ast.While, ast.For)):
                    visit(statement.corpo)

            for child in function.corpo.comandos:
                visit(child)
            repeated.update(names & global_names)
            self.shadowed[function.nome] = repeated

    def _gerar_global(self, declaration):
        self.globais[declaration.nome] = declaration.nome
        if declaration.inicializador is None:
            return
        value = self._expressao(declaration.inicializador)
        self.linhas.append(f"global {declaration.nome} = {value}")

    def _gerar_funcao(self, function):
        self.temporario = 0
        self.rotulos = {}
        self.loop_targets = []
        self.shadowed_names = self.shadowed.get(function.nome, set())
        params = ", ".join(name for _kind, name in function.parametros)
        suffix = f"({params})" if params else ""
        self.linhas.append(f"func {function.nome}{suffix}")
        self.escopos = [{}]
        for _kind, name in function.parametros:
            self.escopos[-1][name] = name
        self._bloco(function.corpo, novo_escopo=False)
        self.linhas.append("endfunc")

    def _novo_temporario(self):
        self.temporario += 1
        return f"t{self.temporario}"

    def _novo_rotulo(self, base):
        numero = self.rotulos.get(base, 0)
        self.rotulos[base] = numero + 1
        return f"L_{base}" if numero == 0 else f"L_{base}_{numero + 1}"

    def _emitir(self, instrucao):
        self.linhas.append(f"  {instrucao}")

    def _emitir_rotulo(self, rotulo):
        self.linhas.append(f"{rotulo}:")

    def _abrir_escopo(self):
        self.escopos.append({})

    def _fechar_escopo(self):
        self.escopos.pop()

    def _declarar(self, declaration):
        name = declaration.nome
        if name not in self.shadowed_names:
            emitted = name
        elif len(self.escopos) == 1:
            emitted = f"{name}#outer"
        else:
            emitted = f"{name}#inner"
        self.escopos[-1][name] = emitted
        return emitted

    def _buscar(self, name):
        for scope in reversed(self.escopos):
            if name in scope:
                return scope[name]
        return self.globais.get(name, name)

    def _bloco(self, block, novo_escopo=True):
        if novo_escopo:
            self._abrir_escopo()
        for statement in block.comandos:
            self._comando(statement)
        if novo_escopo:
            self._fechar_escopo()

    def _ramo(self, statement):
        if isinstance(statement, ast.Block):
            self._bloco(statement)
        else:
            self._abrir_escopo()
            self._comando(statement)
            self._fechar_escopo()

    def _comando(self, statement):
        if isinstance(statement, ast.Block):
            self._bloco(statement)
        elif isinstance(statement, ast.VarDecl):
            target = self._declarar(statement)
            if statement.inicializador is not None:
                value = self._expressao(statement.inicializador)
                self._emitir(f"{target} = {value}")
        elif isinstance(statement, ast.ExprStmt):
            if isinstance(statement.expr, ast.Call) and isinstance(
                statement.expr.callee, ast.Id
            ) and statement.expr.callee.nome in ("print", "read"):
                self._builtin(statement.expr)
            else:
                self._expressao(statement.expr)
        elif isinstance(statement, ast.If):
            self._if(statement)
        elif isinstance(statement, ast.While):
            self._while(statement)
        elif isinstance(statement, ast.For):
            self._for(statement)
        elif isinstance(statement, ast.Break):
            self._emitir(f"goto {self.loop_targets[-1][1]}")
        elif isinstance(statement, ast.Continue):
            self._emitir(f"goto {self.loop_targets[-1][0]}")
        elif isinstance(statement, ast.Return):
            value = self._expressao(statement.expr) if statement.expr is not None else None
            self._emitir("return" if value is None else f"return {value}")

    def _if(self, statement):
        condition = self._expressao(statement.condicao)
        if statement.senao is None:
            end = self._novo_rotulo("end_if")
            self._emitir(f"ifFalse {condition} goto {end}")
            self._ramo(statement.entao)
            self._emitir_rotulo(end)
            return

        alternative = self._novo_rotulo("else")
        end = self._novo_rotulo("end")
        self._emitir(f"ifFalse {condition} goto {alternative}")
        self._ramo(statement.entao)
        self._emitir(f"goto {end}")
        self._emitir_rotulo(alternative)
        self._ramo(statement.senao)
        self._emitir_rotulo(end)

    def _while(self, statement):
        test = self._novo_rotulo("test")
        end = self._novo_rotulo("end")
        self._emitir_rotulo(test)
        condition = self._expressao(statement.condicao)
        self._emitir(f"ifFalse {condition} goto {end}")
        self.loop_targets.append((test, end))
        self._ramo(statement.corpo)
        self.loop_targets.pop()
        self._emitir(f"goto {test}")
        self._emitir_rotulo(end)

    def _for(self, statement):
        if statement.inicializacao is not None:
            self._expressao(statement.inicializacao)
        test = self._novo_rotulo("test")
        end = self._novo_rotulo("end")
        has_continue = self._contains_continue(statement.corpo)
        increment = self._novo_rotulo("increment") if has_continue else test
        self._emitir_rotulo(test)
        if statement.condicao is not None:
            condition = self._expressao(statement.condicao)
            self._emitir(f"ifFalse {condition} goto {end}")
        self.loop_targets.append((increment, end))
        self._ramo(statement.corpo)
        self.loop_targets.pop()
        if has_continue:
            self._emitir_rotulo(increment)
        if statement.incremento is not None:
            self._expressao(statement.incremento)
        self._emitir(f"goto {test}")
        self._emitir_rotulo(end)

    def _contains_continue(self, statement):
        if isinstance(statement, ast.Continue):
            return True
        if isinstance(statement, ast.Block):
            return any(self._contains_continue(child) for child in statement.comandos)
        if isinstance(statement, ast.If):
            return self._contains_continue(statement.entao) or (
                statement.senao is not None
                and self._contains_continue(statement.senao)
            )
        return False

    def _lvalue(self, node):
        if isinstance(node, ast.Id):
            return self._buscar(node.nome)
        if isinstance(node, ast.Index):
            index = self._expressao(node.indice)
            scaled = self._novo_temporario()
            self._emitir(f"{scaled} = {index} * 1")
            return f"{self._expressao(node.base)}[{scaled}]"
        raise ValueError(f"destino IR não atribuível: {node.sexpr()}")

    def _builtin(self, call):
        name = call.callee.nome
        if name == "print":
            for argument in call.args:
                self._emitir(f"print {self._expressao(argument)}")
        elif name == "read":
            for argument in call.args:
                self._emitir(f"read {self._lvalue(argument)}")
        return None

    def _expressao(self, node):
        if node is None:
            return None
        if isinstance(node, ast.Lit):
            if node.tipo in ("char", "string"):
                quote = "'" if node.tipo == "char" else '"'
                return f"{quote}{node.valor}{quote}"
            return str(node.valor)
        if isinstance(node, ast.Id):
            return self._buscar(node.nome)
        if isinstance(node, ast.Index):
            address = self._lvalue(node)
            result = self._novo_temporario()
            self._emitir(f"{result} = load {address}")
            return result
        if isinstance(node, ast.Assign):
            value = self._expressao(node.valor)
            target = self._lvalue(node.alvo)
            if isinstance(node.alvo, ast.Index):
                self._emitir(f"store {target}, {value}")
            else:
                self._emitir(f"{target} = {value}")
            return target
        if isinstance(node, ast.Unary):
            operand = self._expressao(node.expr)
            result = self._novo_temporario()
            operator = "not" if node.op == "!" else "neg"
            self._emitir(f"{result} = {operator} {operand}")
            return result
        if isinstance(node, ast.Binary):
            left = self._expressao(node.esq)
            right = self._expressao(node.dire)
            result = self._novo_temporario()
            operators = {"&&": "and", "||": "or"}
            operator = operators.get(node.op, node.op)
            self._emitir(f"{result} = {left} {operator} {right}")
            return result
        if isinstance(node, ast.Call):
            if isinstance(node.callee, ast.Id) and node.callee.nome in (
                "print",
                "read",
            ):
                return self._builtin(node)
            arguments = [self._expressao(argument) for argument in node.args]
            for argument in arguments:
                self._emitir(f"param {argument}")
            result = self._novo_temporario()
            self._emitir(
                f"{result} = call {self._expressao(node.callee)}, {len(arguments)}"
            )
            return result
        raise TypeError(f"expressão não suportada na geração de IR: {type(node).__name__}")
