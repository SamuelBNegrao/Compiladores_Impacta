# Compiladores

Repositório dos projetos e atividades desenvolvidos na disciplina de **Compiladores** da Faculdade Impacta. O projeto implementa etapas de um compilador para um subconjunto da linguagem C, denominado MiniC: análise léxica, análise sintática, construção de AST e análise semântica.

## Grupo

**Integrantes:**

- Arthur Silva Florentino
- Guilherme de Almeida Cavalcante
- Guilherme Gomes da Silva
- José Henrique Ferreira Pereira
- Natan Miguel Xavier de Araujo
- Paulo Alberto Soares de Oliveira
- Pier Giorgio Moreira Cesar
- Samuel Bertozzi Negrão

**Disciplina:** Compiladores  
**Curso:** Ciência da Computação  
**Instituição:** Faculdade Impacta

## Escopo e estado atual

O fluxo principal em Python é:

```text
código-fonte -> lexer -> parser -> AST -> análise semântica -> diagnósticos
```

- O lexer em C e em Python reconhece tokens, atributos léxicos e posições.
- Os parsers em C e em Python constroem uma AST e identificam erros sintáticos.
- O analisador semântico em Python percorre a AST, mantém escopos e verifica tipos, declarações, chamadas, vetores e retornos.
- O parser usado pelo fluxo semântico existente ([`src/lexer/parser.py`](src/lexer/parser.py)) e o parser C são de descida recursiva. Também há agora uma implementação SLR(1) independente em [`src/lexer/parser_lr.py`](src/lexer/parser_lr.py), que usa uma tabela de estados, ações shift/reduce e reduções com ações semânticas para construir a mesma AST. Ambos os parsers Python são testados contra os mesmos casos.
- O analisador semântico principal continua percorrendo a AST e consultando escopos. Para demonstrar formalmente atributos durante a análise ascendente, [`src/lexer/atributos_lr.py`](src/lexer/atributos_lr.py) aplica uma gramática SLR(1) de expressões aritméticas: o ambiente `amb` é recebido como atributo herdado e `tipo` é sintetizado em cada redução. Essa demonstração cobre expressões aritméticas, não substitui a análise semântica completa do MiniC.
- A geração de código intermediário (IR) ainda não está documentada como etapa implementada neste README.

## Evidências de cobertura e resultados

Os pacotes de teste incluem entradas, resultados esperados e verificações automatizadas:

| Etapa | Evidência | Critério verificado | Resultado executado |
|---|---|---|---|
| Parser Python | 50 casos: 25 válidos e 25 inválidos; AST esperada para cada caso válido | Status de aceitação/rejeição e comparação da AST nos válidos | **50/50 aprovados** |
| Parser ascendente SLR(1) Python | O mesmo conjunto de 50 casos | Status de aceitação/rejeição e comparação da AST nos válidos | **50/50 aprovados** |
| Parser C | O mesmo conjunto de 50 casos | Status de aceitação/rejeição e comparação da AST nos válidos | **50/50 aprovados** |
| Análise semântica Python | 20 programas e arquivos `.gabarito` correspondentes | Saída completa de diagnósticos comparada com o gabarito | **20/20 aprovados** |
| Atributos formais em expressões SLR(1) | Testes automatizados e traço de reduções para `x + 2 * y` | Consulta do ambiente herdado, precedência, tipos sintetizados, traço e regra de `%` | **6/6 testes automatizados aprovados** |

Os resultados acima foram verificados nesta atualização. Os casos e gabaritos estão versionados, permitindo repetir as verificações. A suíte de 50 programas mede a cobertura da sintaxe MiniC contemplada no corpus; os testes unitários da gramática de atributos cobrem adicionalmente precedência aritmética e cálculo de tipos. A demonstração de atributos é intencionalmente delimitada às expressões numéricas.

### Reproduzir parser — PowerShell

Execute os comandos a partir da raiz do repositório:

```powershell
python .\src\lexer\run_tests_parser.py .\src\lexer\parser.py .\src\lexer\testes-parser-50\casos
python .\src\lexer\run_tests_parser.py .\src\lexer\parser.exe .\src\lexer\testes-parser-50\casos --exe
```

O resultado esperado para cada execução é:

```text
Resumo: 50 OK / 0 falharam / 50 total
```

Nos 25 casos válidos, o runner compara a AST produzida com `ast.esperada.txt`. Nos outros 25, verifica que o parser rejeita a entrada. Para conferir o inventário e a descrição dos casos, consulte [`src/lexer/testes-parser-50/README.md`](src/lexer/testes-parser-50/README.md) e [`src/lexer/testes-parser-50/INDICE.txt`](src/lexer/testes-parser-50/INDICE.txt).

### Reproduzir parser ascendente SLR(1)

```powershell
python .\src\lexer\run_tests_parser.py .\src\lexer\parser_lr.py .\src\lexer\testes-parser-50\casos
python .\src\lexer\parser_lr.py .\src\lexer\testes-parser-50\casos\25_programa_integrado\codigo.c --trace
```

O primeiro comando deve terminar com `Resumo: 50 OK / 0 falharam / 50 total`. O segundo imprime a AST em `stdout` e as ações `shift`, `reduce` e `accept` em `stderr`, permitindo observar a análise ascendente. Em uma redução, o parser desempilha os símbolos do lado direito da produção, executa a ação semântica e consulta o estado de destino na tabela `goto`.

### Reproduzir análise semântica Python — PowerShell

O código de saída zero indica programa aceito e o código 1 indica programa rejeitado. O comando abaixo compara cada saída com seu gabarito UTF-8 e interrompe a execução na primeira divergência:

```powershell
$casos = Get-ChildItem .\src\tests\semantic -Filter *.c | Sort-Object Name
foreach ($caso in $casos) {
    $gabarito = (Get-Content ($caso.FullName -replace '\.c$', '.gabarito') -Raw -Encoding UTF8).Trim()
    $saida = (& python .\src\semantic\python\semantic.py $caso.FullName 2>&1 | Out-String).Trim()
    if ($saida -cne $gabarito) {
        throw "Divergência no caso $($caso.Name)"
    }
}
"$($casos.Count)/$($casos.Count) casos aprovados"
```

Resultado esperado com o conjunto atual:

```text
20/20 casos aprovados
```

### Reproduzir demonstração de atributos

```powershell
python .\src\lexer\atributos_lr.py "x + 2 * y" --env "x=int,y=float" --trace
python -m unittest discover -s .\src\lexer -p test_lr.py
```

O primeiro comando deve produzir a AST `Binary(+,Id(x),Binary(*,Lit(int,2),Id(y)))` e `tipo sintetizado: float`. O traço de reduções mostra `tipo=int` para `x` e `2`, `tipo=float` para `2 * y` e, por fim, `tipo=float` para a soma. O segundo comando executa os seis testes automatizados do parser e dos atributos.

## Atributos semânticos: especificação e avaliação

Na demonstração, `amb` é um atributo **herdado**: o contexto fornece a tabela de identificadores e seus tipos, e o mesmo ambiente é propagado para os operandos. `tipo` é um atributo **sintetizado** pelas reduções, usando os atributos dos operandos. As regras implementadas para a gramática aritmética incluem:

```text
Expr -> Expr1 + Term
    Expr1.amb = Expr.amb
    Term.amb = Expr.amb
    Expr.tipo = promover(Expr1.tipo, Term.tipo)

Term -> Term1 * Factor
    Term1.amb = Term.amb
    Factor.amb = Term.amb
    Term.tipo = promover(Term1.tipo, Factor.tipo)

Factor -> id
    Factor.tipo = lookup(Factor.amb, id.lexema)
    Erro se id.lexema não pertencer a Factor.amb

Term -> Term1 % Factor
    Term1.amb = Term.amb
    Factor.amb = Term.amb
    Erro se Term1.tipo != int ou Factor.tipo != int
    Term.tipo = int

Regra de promoção:
promover(int, int)     = int
promover(float, int)   = float
promover(int, float)   = float
promover(float, float) = float
```

Em [`src/lexer/atributos_lr.py`](src/lexer/atributos_lr.py), a função `analisar` recebe explicitamente `amb`; a redução `Factor -> IDENT` consulta esse ambiente e cria `ExpressaoTipada`. Nas reduções aritméticas, a ação lê os tipos sintetizados dos filhos, verifica a regra e sintetiza o tipo do nó pai. O modo `--trace` exibe esses valores durante a execução, vinculando a especificação ao comportamento observável. A gramática completa do parser SLR(1) e suas ações que constroem a AST estão em [`src/lexer/parser_lr.py`](src/lexer/parser_lr.py); o algoritmo da tabela SLR(1) e da pilha shift/reduce está em [`src/lexer/lr.py`](src/lexer/lr.py).

## Estrutura do projeto

```text
Compiladores_Impacta/
├── artifacts/
│   ├── lexer-c/                 # saídas e relatórios do scanner C
│   └── lexer-python/            # saídas e relatórios do scanner Python
├── src/
│   ├── lexer/                   # lexer e parsers C/Python
│   │   └── testes-parser-50/    # casos de parser, ASTs esperadas e índice
│   ├── semantic/                # análise semântica C/Python e runners
│   └── tests/
│       ├── minic/               # fixtures de lexer e documentação
│       └── semantic/            # 20 fontes semânticas e gabaritos
└── README.md
```

Mais detalhes sobre o scanner estão em [`src/tests/minic/README.md`](src/tests/minic/README.md), sobre o parser em [`src/lexer/testes-parser-50/README.md`](src/lexer/testes-parser-50/README.md) e sobre a análise semântica em [`src/semantic/README_SEMANTICO.md`](src/semantic/README_SEMANTICO.md).
