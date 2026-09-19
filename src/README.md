```bash
# A partir de src/lexer
python run_tests_parser.py parser.py testes-parser-50\casos


gcc -DLEXER_SEM_MAIN parser.c lexer_v2.c -o parser.exe     
>> python run_tests_parser.py parser.exe testes-parser-50\casos --exe
