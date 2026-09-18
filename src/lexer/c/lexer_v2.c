#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "lexer_v2.h"

// MAPEAMENTOS
typedef struct { const char *chave; const char *valor; } Par;

const Par PALAVRAS_RESERVADAS[] = {
    {"int", "INT"}, {"float", "FLOAT"}, {"bool", "BOOL"}, {"char", "CHAR"}, {"void", "VOID"},
    {"if", "IF"}, {"else", "ELSE"}, {"while", "WHILE"}, {"for", "FOR"},
    {"return", "RETURN"}, {"break", "BREAK"}, {"continue", "CONTINUE"},
    {"true", "TRUE"}, {"false", "FALSE"}, {"print", "PRINT"}, {"read", "READ"},
    {NULL, NULL}
};

const Par OPERADORES[] = {
    {"==", "EQ"}, {"!=", "NE"}, {"<=", "LE"}, {">=", "GE"},
    {"&&", "AND"}, {"||", "OR"}, {"+", "PLUS"}, {"-", "MINUS"},
    {"*", "STAR"}, {"/", "SLASH"}, {"%", "PERCENT"}, {"<", "LT"},
    {">", "GT"}, {"!", "NOT"}, {"=", "ASSIGN"}, {NULL, NULL}
};

const Par DELIMITADORES[] = {
    {"(", "LPAREN"}, {")", "RPAREN"},
    {"[", "LBRACKET"}, {"]", "RBRACKET"},
    {"{", "LBRACE"}, {"}", "RBRACE"},
    {";", "SEMICOLON"}, {",", "COMMA"},
    {".", "DOT"},
    {NULL, NULL}
};

// LEXER
typedef struct {
    const char *codigo;
    int pos;
    int tamanho;
    int linha;
    int coluna;
} Lexer;

char lexer_atual(Lexer *l) {
    if (l->pos >= l->tamanho) return '\0';
    return l->codigo[l->pos];
}

char lexer_proximo(Lexer *l) {
    if (l->pos + 1 >= l->tamanho) return '\0';
    return l->codigo[l->pos + 1];
}

char lexer_avancar(Lexer *l) {
    if (l->pos >= l->tamanho) return '\0';
    char c = l->codigo[l->pos++];
    if (c == '\n') {
        l->linha++;
        l->coluna = 1;
    } else {
        l->coluna++;
    }
    return c;
}

const char* buscar_par(const Par *tabela, const char *chave) {
    for (int i = 0; tabela[i].chave != NULL; i++) {
        if (strcmp(tabela[i].chave, chave) == 0) return tabela[i].valor;
    }
    return NULL;
}

void emitir_json(Token t) {
    printf("{\"token\": \"%s\", \"lexeme\": \"", t.tipo);
    for (int i = 0; t.lexema[i] != '\0'; i++) {
        if (t.lexema[i] == '"') printf("\\\"");
        else if (t.lexema[i] == '\\') printf("\\\\");
        else printf("%c", t.lexema[i]);
    }
    printf("\", \"attribute\": ");

    if (t.eh_null) {
        printf("null");
    } else if (t.tem_atributo_num) {
        if (strcmp(t.tipo, "INT_LIT") == 0) {
            printf("%ld", (long)t.atributo_num);
        } else {
            printf("%.1f", t.atributo_num);
        }
    } else {
        printf("\"");
        for (int i = 0; t.atributo[i] != '\0'; i++) {
            if (t.atributo[i] == '"') printf("\\\"");
            else if (t.atributo[i] == '\\') printf("\\\\");
            else printf("%c", t.atributo[i]);
        }
        printf("\"");
    }
    printf(", \"line\": %d, \"column\": %d}\n", t.linha, t.coluna);
}

/* ---- Armazenamento em array (antes: imprimia direto via emitir_json) ---- */

static void token_list_reservar(TokenList *lista, int minimo) {
    if (lista->capacidade >= minimo) return;
    int nova_cap = lista->capacidade == 0 ? 64 : lista->capacidade * 2;
    while (nova_cap < minimo) nova_cap *= 2;
    lista->itens = (Token *)realloc(lista->itens, nova_cap * sizeof(Token));
    lista->capacidade = nova_cap;
}

void adicionar_token(TokenList *lista, const char *tipo, const char *lexema, int linha, int coluna) {
    token_list_reservar(lista, lista->quantidade + 1);
    Token *t = &lista->itens[lista->quantidade];

    if (strcmp(tipo, "IDENTIFICADOR") == 0) {
        strcpy(t->tipo, "IDENT");
    } else {
        strcpy(t->tipo, tipo);
    }

    strcpy(t->lexema, lexema);

    t->linha = linha;
    t->coluna = coluna;
    t->tem_atributo_num = false;
    t->eh_null = false;
    t->atributo[0] = '\0';
    t->atributo_num = 0.0;

    if (strcmp(tipo, "INT_LIT") == 0 || strcmp(tipo, "FLOAT_LIT") == 0) {
        t->tem_atributo_num = true;
        t->atributo_num = atof(lexema);
    } else if (strcmp(tipo, "CHAR_LIT") == 0 || strcmp(tipo, "STRING_LIT") == 0) {
        int len = strlen(lexema);
        if (len >= 2 && lexema[0] == lexema[len - 1] && (lexema[0] == '"' || lexema[0] == '\'')) {
            strncpy(t->atributo, lexema + 1, len - 2);
            t->atributo[len - 2] = '\0';
        } else if (len >= 1 && (lexema[0] == '"' || lexema[0] == '\'')) {
            strcpy(t->atributo, lexema + 1);
        } else {
            strcpy(t->atributo, lexema);
        }
    } else if (strcmp(t->tipo, "IDENT") == 0) {
        strcpy(t->atributo, lexema);
    } else {
        t->eh_null = true;
    }

    lista->quantidade++;
}

void analisar(Lexer *l, TokenList *lista) {
    while (l->pos < l->tamanho) {
        char c = lexer_atual(l);

        if (isspace((unsigned char)c)) {
            lexer_avancar(l);
            continue;
        }

        // Comentários
        if (c == '/' && (lexer_proximo(l) == '/' || lexer_proximo(l) == '*')) {
            if (lexer_proximo(l) == '/') {
                lexer_avancar(l); lexer_avancar(l);
                while (l->pos < l->tamanho && lexer_atual(l) != '\n') lexer_avancar(l);
            } else {
                lexer_avancar(l); lexer_avancar(l);
                while (l->pos < l->tamanho) {
                    if (lexer_atual(l) == '*' && lexer_proximo(l) == '/') {
                        lexer_avancar(l); lexer_avancar(l);
                        break;
                    }
                    lexer_avancar(l);
                }
            }
            continue;
        }

        // Identificadores e Palavras Reservadas
        if (isalpha((unsigned char)c) || c == '_') {
            int l_ini = l->linha, c_ini = l->coluna, inicio = l->pos;
            lexer_avancar(l);
            while (isalnum((unsigned char)lexer_atual(l)) || lexer_atual(l) == '_') lexer_avancar(l);

            int len = l->pos - inicio;
            char lexema[256];
            strncpy(lexema, l->codigo + inicio, len);
            lexema[len] = '\0';

            const char *res = buscar_par(PALAVRAS_RESERVADAS, lexema);
            adicionar_token(lista, res ? res : "IDENTIFICADOR", lexema, l_ini, c_ini);
            continue;
        }

        // Números
        if (isdigit((unsigned char)c)) {
            int l_ini = l->linha, c_ini = l->coluna, inicio = l->pos;
            while (isdigit((unsigned char)lexer_atual(l))) lexer_avancar(l);

            bool eh_float = false;
            if (lexer_atual(l) == '.' && isdigit((unsigned char)lexer_proximo(l))) {
                eh_float = true;
                lexer_avancar(l);
                while (isdigit((unsigned char)lexer_atual(l))) lexer_avancar(l);
            }

            int len = l->pos - inicio;
            char lexema[256];
            strncpy(lexema, l->codigo + inicio, len);
            lexema[len] = '\0';

            adicionar_token(lista, eh_float ? "FLOAT_LIT" : "INT_LIT", lexema, l_ini, c_ini);
            continue;
        }

        // Caractere Literal
        if (c == '\'') {
            int l_ini = l->linha, c_ini = l->coluna, inicio = l->pos;
            lexer_avancar(l);

            while (l->pos < l->tamanho && lexer_atual(l) != '\'' && lexer_atual(l) != '\n') {
                if (lexer_atual(l) == '\\') lexer_avancar(l);
                lexer_avancar(l);
            }

            if (lexer_atual(l) == '\'') {
                lexer_avancar(l);
                int len = l->pos - inicio;
                char lexema[256];
                strncpy(lexema, l->codigo + inicio, len);
                lexema[len] = '\0';
                adicionar_token(lista, "CHAR_LIT", lexema, l_ini, c_ini);
            }
            continue;
        }

        // String Literal (Trata encerramento precoce sem engolir a instrucao)
        if (c == '"') {
            int l_ini = l->linha, c_ini = l->coluna, inicio = l->pos;
            lexer_avancar(l);

            while (l->pos < l->tamanho && lexer_atual(l) != '"' && lexer_atual(l) != '\n') {
                // Interrompe se encontrar caracteres de fechamento de instrução sem fechar aspas
                if (lexer_atual(l) == ')' || lexer_atual(l) == ';') {
                    break;
                }
                if (lexer_atual(l) == '\\') lexer_avancar(l);
                lexer_avancar(l);
            }

            if (lexer_atual(l) == '"') {
                lexer_avancar(l);
                int len = l->pos - inicio;
                char lexema[256];
                strncpy(lexema, l->codigo + inicio, len);
                lexema[len] = '\0';
                adicionar_token(lista, "STRING_LIT", lexema, l_ini, c_ini);
            }
            continue;
        }

        // Operadores de 2 Caracteres
        char duplo[3] = {c, lexer_proximo(l), '\0'};
        const char *op_duplo = buscar_par(OPERADORES, duplo);
        if (op_duplo) {
            int l_ini = l->linha, c_ini = l->coluna;
            lexer_avancar(l); lexer_avancar(l);
            adicionar_token(lista, op_duplo, duplo, l_ini, c_ini);
            continue;
        }

        // Operadores de 1 Caractere
        char simples[2] = {c, '\0'};
        const char *op_simples = buscar_par(OPERADORES, simples);
        if (op_simples) {
            int l_ini = l->linha, c_ini = l->coluna;
            lexer_avancar(l);
            adicionar_token(lista, op_simples, simples, l_ini, c_ini);
            continue;
        }

        // Delimitadores
        const char *del = buscar_par(DELIMITADORES, simples);
        if (del) {
            int l_ini = l->linha, c_ini = l->coluna;
            lexer_avancar(l);
            adicionar_token(lista, del, simples, l_ini, c_ini);
            continue;
        }

        lexer_avancar(l);
    }

    // Token obrigatório EOF
    adicionar_token(lista, "EOF", "", l->linha, l->coluna);
}

/* ---- API pública (declarada em lexer_v2.h) — usada pelo main() abaixo
 * e também pelo parser.c ---- */

TokenList lexer_tokenizar(const char *codigo, int tamanho) {
    TokenList lista = {0};
    Lexer l = { codigo, 0, tamanho, 1, 1 };
    analisar(&l, &lista);
    return lista;
}

void token_list_liberar(TokenList *lista) {
    free(lista->itens);
    lista->itens = NULL;
    lista->quantidade = 0;
    lista->capacidade = 0;
}

#ifndef LEXER_SEM_MAIN
int main(int argc, char *argv[]) {
    if (argc < 2) return 1;

    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;

    fseek(f, 0, SEEK_END);
    int tamanho = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *codigo = (char *)malloc(tamanho + 1);
    fread(codigo, 1, tamanho, f);
    codigo[tamanho] = '\0';
    fclose(f);

    TokenList lista = lexer_tokenizar(codigo, tamanho);
    for (int i = 0; i < lista.quantidade; i++) {
        emitir_json(lista.itens[i]);
    }

    token_list_liberar(&lista);
    free(codigo);
    return 0;
}
#endif
