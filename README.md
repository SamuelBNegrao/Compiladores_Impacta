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
- O analisador semântico em Python percorre a AST, mantém escopos e verifica tipos, declarações, chamadas, vetores, retornos e o uso contextual de `break`/`continue`.
- O parser usado pelo fluxo semântico existente ([`src/lexer/parser.py`](src/lexer/parser.py)) e o parser C são de descida recursiva. Também há agora uma implementação SLR(1) independente em [`src/lexer/parser_lr.py`](src/lexer/parser_lr.py), que usa uma tabela de estados, ações shift/reduce e reduções com ações semânticas para construir a mesma AST. Ambos os parsers Python são testados contra os mesmos casos.
- O analisador semântico principal continua percorrendo a AST e consultando escopos. Para demonstrar formalmente atributos durante a análise ascendente, [`src/lexer/atributos_lr.py`](src/lexer/atributos_lr.py) aplica uma gramática SLR(1) de expressões aritméticas: o ambiente `amb` é recebido como atributo herdado e `tipo` é sintetizado em cada redução. Essa demonstração cobre expressões aritméticas, não substitui a análise semântica completa do MiniC.
- Após a análise semântica, os geradores em [`src/semantic/python/ir.py`](src/semantic/python/ir.py) e [`src/semantic/c/ir.c`](src/semantic/c/ir.c) emitem uma representação intermediária de três endereços (`-O0`), com temporários e rótulos explícitos. As CLIs integram essas etapas após a análise; no C, a AST compartilhada é definida em [`src/lexer/parser.h`](src/lexer/parser.h). O otimizador C fica isolado em [`src/semantic/c/optimizer.c`](src/semantic/c/optimizer.c), separado do gerador e da CLI. Ambos cobrem funções, expressões, chamadas, vetores, condicionais, laços (`while`/`for`), `break`, `continue` e `return`. Em `-O1`, aplicam propagação e dobra de constantes, simplificação algébrica, propagação de cópias, eliminação de atribuições mortas e código inalcançável, subexpressões comuns locais, simplificação de condições constantes e movimento conservador de expressões invariantes de laço. Redução de força em variáveis de indução não é aplicada.

## Evidências de cobertura e resultados

Os pacotes de teste incluem entradas, resultados esperados e verificações automatizadas:

| Etapa | Evidência | Critério verificado | Resultado executado |
|---|---|---|---|
| Parser Python | 50 casos: 25 válidos e 25 inválidos; AST esperada para cada caso válido | Status de aceitação/rejeição e comparação da AST nos válidos | **50/50 aprovados** |
| Parser ascendente SLR(1) Python | O mesmo conjunto de 50 casos | Status de aceitação/rejeição e comparação da AST nos válidos | **50/50 aprovados** |
| Parser C | O mesmo conjunto de 50 casos | Status de aceitação/rejeição e comparação da AST nos válidos | **50/50 aprovados** |
| Análise semântica Python | 20 programas e arquivos `.gabarito` correspondentes | Saída completa de diagnósticos comparada com o gabarito | **20/20 aprovados** |
| Atributos formais em expressões SLR(1) | Testes automatizados e traço de reduções para `x + 2 * y` | Consulta do ambiente herdado, precedência, tipos sintetizados, traço e regra de `%` | **6/6 testes automatizados aprovados** |
| IR Python `-O0` e `-O1` | 20 programas válidos em `-O0`, 10 inválidos, 10 alvos O1 e testes unitários | Aceitação/emissão básica em O0; rejeição/categoria nos inválidos; verificações dos resultados otimizados observáveis, das passagens de IR e das combinações de opções da CLI | **40/40 verificações da suíte; 20 testes unitários aprovados** |
| IR C `-O0` e `-O1` | 30 casos integrados, 10 alvos O1, 2 fluxos adicionais, 3 combinações de opções, 1 caso lógico e 1 caso CSE | Aceitação/emissão em O0; códigos esperados nos inválidos; critérios dos alvos O1, fluxos de `break`/`continue`, rejeição de opções incompatíveis, simplificação lógica e CSE | **47/47 verificações aprovadas** |

Os resultados acima foram verificados nesta atualização. Os casos e gabaritos estão versionados, permitindo repetir as verificações. A suíte de 50 programas mede a cobertura da sintaxe MiniC contemplada no corpus; os testes unitários da gramática de atributos cobrem adicionalmente precedência aritmética e cálculo de tipos. A demonstração de atributos é intencionalmente delimitada às expressões numéricas.

A suíte de 30 casos de IR está integrada em [`src/semantic/python/testes-ir/`](src/semantic/python/testes-ir/). Seus arquivos `.txt` descrevem critérios e IR canônicas de referência; os runners não comparam a IR produzida linha a linha com essas referências nem executam a IR em uma máquina virtual. Em `-O0`, verificam aceitação/rejeição, códigos de saída e presença de uma função `main`; em `-O1`, verificam critérios selecionados por caso (por exemplo, resultado constante observável, remoção de código inalcançável e movimento de uma expressão invariável). Os testes unitários e o caso suplementar de CSE exercitam passagens específicas. Esses resultados dão evidência direcionada das transformações, mas não provam equivalência semântica para todos os programas MiniC nem para todos os fluxos de controle possíveis.

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

### Reproduzir geração de IR `-O0`

Na raiz do repositório, execute os testes unitários da IR e compile um programa:

```powershell
python -m unittest discover -s .\src\semantic\python -p test_ir.py
python .\src\semantic\python\minic.py .\caminho\para\programa.c -O0
python .\src\semantic\python\minic.py .\caminho\para\programa.c --check
```

`-O0` (também o modo padrão e `--ir`) emite a IR sem otimizações; `-O1` emite a IR após as passagens de otimização implementadas; `--check` executa as etapas léxica, sintática e semântica sem emitir IR. As duas CLIs retornam `0` para compilação/validação bem-sucedida, `2` para erro léxico, `3` para erro sintático e `4` para erro semântico.

#### O que significa `-O0` e `-O1`?

O `O` é a letra “ó”, de *optimization* (otimização), não o algarismo zero. `-O0` indica compilar sem aplicar otimizações; neste projeto, é o nível que emite a IR básica. `-O1` aplica um primeiro conjunto de transformações sobre a IR: propagação/dobra de constantes, simplificação algébrica, propagação de cópias, eliminação de atribuições mortas e código inalcançável, subexpressões comuns locais, condições constantes e movimento conservador de código invariável em laços.

Otimizações podem reduzir instruções e temporários, diminuir trabalho em tempo de execução e preparar melhor o código para etapas posteriores. Elas também tornam o compilador mais complexo e exigem testes para garantir que a versão otimizada mantenha a semântica da não otimizada. Neste projeto, `-O1` está implementado tanto em Python quanto em C. O caso O09 descreve redução de força em variável de indução como uma possibilidade, não como requisito; esta transformação não é feita. Os runners verificam resultados e estruturas escolhidos, mas não executam as IRs nem demonstram equivalência geral em toda a linguagem.

Para executar os casos integrados em `-O0`, os critérios direcionados O01–O10 em `-O1` e os erros, rode:

```powershell
python .\src\semantic\python\run_ir_suite.py
```

O runner valida 40 verificações: emissão básica para as 20 entradas válidas, códigos de saída para as 10 inválidas e critérios específicos para cada caso O01–O10 otimizado. O09 só exige que a IR otimizada continue válida, pois redução de força é opcional. Os arquivos `.txt` mantêm os critérios e IR canônicas para inspeção; os runners não executam a IR nem fazem comparação textual byte a byte.

### Reproduzir geração de IR em C

Com GCC e Python disponíveis, execute da raiz do repositório:

```powershell
python .\src\semantic\c\run_ir_suite.py
```

O runner compila `minic.c`, `ir.c` e `optimizer.c` em um diretório temporário e executa os mesmos casos em `-O0` e os critérios O01–O10 em `-O1`, além de verificações de `continue`, `break`, combinações incompatíveis com `--check`, simplificação de expressão lógica e eliminação de subexpressão comum, sem deixar executáveis gerados no repositório. Também é possível compilar e invocar a CLI diretamente:

```powershell
gcc -Wall -Wextra -std=c11 .\src\semantic\c\minic.c .\src\semantic\c\ir.c .\src\semantic\c\optimizer.c -o .\src\semantic\c\minic.exe
.\src\semantic\c\minic.exe --ir .\src\semantic\python\testes-ir\testes\01-validos-sem-otimizacao\P01\P01.c
```

Sem `--ir`, `-O0` ou `-O1`, a CLI C preserva a saída anterior de análise semântica. Com `--ir`/`-O0`/`-O1`, os códigos de saída são 0 (sucesso), 2 (léxico), 3 (sintático) e 4 (semântico). `--check` valida sem emitir IR.

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
│   │   ├── parser.h             # AST compartilhada com o gerador de IR C
│   │   └── testes-parser-50/    # casos de parser, ASTs esperadas e índice
│   ├── semantic/                # análise semântica, IR e runners
│   │   ├── c/
│   │   │   ├── minic.c          # CLI e análise semântica do compilador C
│   │   │   ├── ir.c / ir.h      # gerador C da IR
│   │   │   ├── optimizer.c / optimizer.h # otimizador C da IR para -O1
│   │   │   └── run_ir_suite.py  # runner C dos casos de IR
│   │   └── python/
│   │       ├── minic.py         # CLI integrada de compilação MiniC
│   │       ├── ir.py            # gerador de IR de três endereços
│   │       ├── optimizer.py     # otimizações de IR para -O1
│   │       ├── run_ir_suite.py  # runner dos 30 casos integrados
│   │       ├── testes-ir/       # fontes e referências da suíte de IR
│   │       └── test_ir.py       # testes unitários da IR e CLI
│   └── tests/
│       ├── minic/               # fixtures de lexer e documentação
│       └── semantic/            # 20 fontes semânticas e gabaritos
└── README.md
```

Mais detalhes sobre o scanner estão em [`src/tests/minic/README.md`](src/tests/minic/README.md), sobre o parser em [`src/lexer/testes-parser-50/README.md`](src/lexer/testes-parser-50/README.md) e sobre a análise semântica em [`src/semantic/README_SEMANTICO.md`](src/semantic/README_SEMANTICO.md).
