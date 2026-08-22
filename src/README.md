```bash
# Execução dos testes para o Analisador Léxico em C
cd ../LexerC
./test_scanner_c.sh lexer_v2.c ../testes-scanner-minic_codes/casos-invalidos
./test_scanner_c.sh lexer_v2.c ../testes-scanner-minic_codes/casos-programas-c

# Execução dos testes para o Analisador Léxico em Python
cd ../LexerPython
bash ./test_scanner_python.sh lexer_v2.py ../testes-scanner-minic_codes/casos-programas-c
bash ./test_scanner_python.sh lexer_v2.py ../testes-scanner-minic_codes/casos-invalidos
