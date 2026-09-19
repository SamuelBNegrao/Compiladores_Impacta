```bash
# A partir de src/lexer
bash c/test_scanner_c.sh lexer_v2.c ./testes-scanner-minic_codes/casos-invalidos
bash c/test_scanner_c.sh lexer_v2.c ./testes-scanner-minic_codes/casos-programas-c
bash python/test_scanner_python.sh lexer_v2.py ./testes-scanner-minic_codes/casos-programas-c
bash python/test_scanner_python.sh lexer_v2.py ./testes-scanner-minic_codes/casos-invalidos

gcc -Wall -Wextra -std=c11 -DLEXER_SEM_MAIN parser.c lexer_v2.c -o ../../parser
../../parser ./testes-scanner-minic_codes/casos-programas-c/c01_fibonacci.c

# Suítes dos parsers
cd src/lexer
bash testar_parser_python.sh ./testes-parser-50 ./parser.py
bash testar_parser_c.sh ./testes-parser-50 ./parser.c
