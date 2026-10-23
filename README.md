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

O fluxo implementado em Python é:

```text
código-fonte -> lexer -> parser -> AST -> análise semântica -> diagnósticos
```

- O lexer em C e em Python reconhece tokens, atributos léxicos e posições.
- Os parsers em C e em Python constroem uma AST e identificam erros sintáticos.
- O analisador semântico em Python percorre a AST, mantém escopos e verifica tipos, declarações, chamadas, vetores e retornos.
- **O parser atualmente implementado é de descida recursiva (top-down), não um parser ascendente LR.** A implementação Python distribui a análise por métodos como `parse_expr`, `parse_additive` e `parse_primary`; o parser C também usa funções `parse_*`. Portanto, os testes do parser demonstram aceitação/rejeição e estrutura da AST, mas não demonstram análise ascendente, ações de shift/reduce ou uso de uma tabela LR.
- A análise semântica calcula tipos durante a visita à AST e consulta uma pilha de tabelas de símbolos. Isso permite relacionar o código com atributos herdados e sintetizados, mas ainda não há uma especificação formal de atributos associada a cada produção da gramática nem uma demonstração de avaliação desses atributos no parser.
- A geração de código intermediário (IR) ainda não está documentada como etapa implementada neste README.

## Evidências de cobertura e resultados

Os pacotes de teste incluem entradas, resultados esperados e verificações automatizadas:

| Etapa | Evidência | Critério verificado | Resultado executado |
|---|---|---|---|
| Parser Python | 50 casos: 25 válidos e 25 inválidos; AST esperada para cada caso válido | Status de aceitação/rejeição e comparação da AST nos válidos | **50/50 aprovados** |
| Parser C | O mesmo conjunto de 50 casos | Status de aceitação/rejeição e comparação da AST nos válidos | **50/50 aprovados** |
| Análise semântica Python | 20 programas e arquivos `.gabarito` correspondentes | Saída completa de diagnósticos comparada com o gabarito | **20/20 aprovados** |

Os resultados acima foram verificados nesta atualização. Os casos e gabaritos estão versionados, permitindo repetir as verificações. Eles demonstram a cobertura funcional descrita na tabela; não constituem, isoladamente, evidência de parser ascendente nem de uma gramática de atributos formalmente implementada.

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

## Atributos semânticos: modelo formal e relação com o código

Uma forma de formalizar a verificação de tipos é atribuir a cada expressão um atributo **sintetizado** `tipo`. A consulta ao ambiente de declarações é representada por um atributo **herdado** `amb`, recebido do contexto. Por exemplo, para expressões aritméticas:

```text
Produção: E -> E1 + E2
Condição:  E1.tipo e E2.tipo são numéricos
Regra:     E.tipo = float, se E1.tipo = float ou E2.tipo = float;
           caso contrário, E.tipo = int.
Erro:      se algum operando não for numérico.

Produção: E -> id
Regra:     E.tipo = buscar(E.amb, id.lexema).tipo
Erro:      se id não estiver declarado em E.amb.
```

`tipo` é sintetizado a partir dos filhos da expressão; `amb` é herdado do contexto de análise. Na implementação Python, `expr` e os métodos `e_*` retornam instâncias de `Tipo`, enquanto `buscar` consulta os escopos ativos. Essa correspondência explica o cálculo semântico presente no código, mas o modelo acima é uma **formalização ilustrativa**, não uma gramática de atributos já executada pelo parser.

Para atender integralmente à demonstração solicitada de análise ascendente e atributos formais, ainda é necessário implementar ou integrar um parser LR (com gramática e ações/traços de shift-reduce), especificar as regras de atributos por produção e incluir evidências de execução que mostrem os atributos fluindo na análise. Até essa etapa, o projeto não deve ser descrito como tendo parser ascendente.

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
