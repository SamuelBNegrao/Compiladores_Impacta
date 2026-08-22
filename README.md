# Compiladores

Repositório destinado aos projetos, atividades e trabalhos desenvolvidos na disciplina de **Compiladores** da faculdade.

## 👥 Grupo

**Integrantes:**

* `Arthur Silva Florentino`
* `Guilherme de Almeida Cavalcante`
* `Guilherme Gomes da Silva`
* `José Henrique Ferreira Pereira`
* `Natan Miguel Xavier de Araujo`
* `Paulo Alberto Soares de Oliveira`
* `Pier Giorgio Moreira Cesar`
* `Samuel Bertozzi Negrão`

## 📚 Disciplina

* **Matéria:** Compiladores
* **Curso:** Ciência da Computação
* **Instituição:** Faculdade Impacta

## 🎯 Objetivo

Este repositório tem como objetivo armazenar e documentar os trabalhos e atividades realizados durante a disciplina, envolvendo conceitos relacionados à construção de compiladores, análise léxica, análise sintática e demais etapas do processo de compilação.

### Comandos de Execução dos Testes

```bash
# Execução dos testes para o Analisador Léxico em C
cd ../LexerC
./test_scanner_c.sh lexer_v2.c ../testes-scanner-minic_codes/casos-invalidos
./test_scanner_c.sh lexer_v2.c ../testes-scanner-minic_codes/casos-programas-c

# Execução dos testes para o Analisador Léxico em Python
cd ../LexerPython
bash ./test_scanner_python.sh lexer_v2.py ../testes-scanner-minic_codes/casos-programas-c
bash ./test_scanner_python.sh lexer_v2.py ../testes-scanner-minic_codes/casos-invalidos

---

## 📂 Estrutura do Projeto

```text
📦 src
 ┣ 📂 LexerC
 ┃ ┣ 📜 lexer_v2.c
 ┃ ┣ 📜 test_scanner_c.sh
 ┃ ┣ 📜 lexer_v2.exe
 ┃ ┣ 📜 scanner.exe
 ┃ ┗ 📂 resultados
 ┃ ┃ ┣ 📜 execucao_testes.txt
 ┃ ┃ ┣ 📜 casos-invalidos.jsonl
 ┃ ┃ ┗ 📜 casos-programas-c.jsonl
 ┣ 📂 LexerPython
 ┃ ┣ 📜 lexer_v2.py
 ┃ ┣ 📜 test_scanner_python.sh
 ┃ ┗ 📂 resultados
 ┃ ┃ ┣ 📜 execucao_testes.txt
 ┃ ┃ ┣ 📜 casos-invalidos.jsonl
 ┃ ┃ ┗ 📜 casos-programas-c.jsonl
 ┣ 📂 testes-scanner-minic_codes
 ┃ ┣ 📂 casos-invalidos
 ┃ ┗ 📂 casos-programas-c
 ┃ ┣ 📜 MANIFESTO.md
 ┃ ┣ 📜 README.md
 ┃ ┣ 📜 check_fixtures.py
 ┗ 📜 README.md



