#ifndef LEXER_V2_H
#define LEXER_V2_H

#include <stdbool.h>

typedef struct {
    char tipo[32];        // ex: "IDENT", "INT_LIT", "IF", "LPAREN", "EOF"...
    char lexema[256];      // texto exato reconhecido no código-fonte
    char atributo[256];    // atributo textual (ident/char/string) quando aplicável
    bool tem_atributo_num; // true para INT_LIT/FLOAT_LIT
    double atributo_num;   // valor numérico quando tem_atributo_num
    bool eh_null;          // true quando o token não tem atributo (ex: operadores)
    int linha;
    int coluna;
} Token;

typedef struct {
    Token *itens;
    int quantidade;
    int capacidade;
} TokenList;

/* Executa a análise léxica completa sobre 'codigo' (buffer com 'tamanho'
 * bytes) e retorna a lista de tokens, sempre terminada por um token "EOF".
 * O chamador é dono da memória retornada e deve liberar com
 * token_list_liberar(). Usada tanto pelo scanner (main deste arquivo)
 * quanto pelo parser (parser.c inclui este header). */
TokenList lexer_tokenizar(const char *codigo, int tamanho);

void token_list_liberar(TokenList *lista);

#endif
