```bash
# Execução dos testes para o Analisador Léxico em C
cd lexer/c
./test_scanner_c.sh lexer_v2.c ../../tests/minic/casos-invalidos
./test_scanner_c.sh lexer_v2.c ../../tests/minic/casos-programas-c

# Execução dos testes para o Analisador Léxico em Python
cd ../python
bash ./test_scanner_python.sh lexer_v2.py ../../tests/minic/casos-programas-c
bash ./test_scanner_python.sh lexer_v2.py ../../tests/minic/casos-invalidos

# Execução do parser Python nos programas válidos
for arquivo in ../../tests/minic/casos-programas-c/*.c; do
	python parser.py "$arquivo" > /dev/null || exit 1
done

# Build do parser C (binário gerado na raiz do projeto)
cd ../..
gcc -Wall -Wextra -std=c11 -DLEXER_SEM_MAIN \
	src/lexer/c/parser.c src/lexer/c/lexer_v2.c -o parser
./parser src/tests/minic/casos-programas-c/c01_fibonacci.c

# Suíte do parser C, quando disponível
bash src/lexer/c/testar_parser_c.sh src/tests/parser-50 src/lexer/c/parser.c
