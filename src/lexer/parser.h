#ifndef MINIC_PARSER_H
#define MINIC_PARSER_H

#include "lexer_v2.h"

#include <stdbool.h>

typedef enum {
    N_PROGRAM,
    N_FUNCTION,
    N_BLOCK,
    N_VARDECL,
    N_IF,
    N_WHILE,
    N_FOR,
    N_BREAK,
    N_CONTINUE,
    N_RETURN,
    N_EXPRSTMT,
    N_ASSIGN,
    N_BINARY,
    N_UNARY,
    N_CALL,
    N_INDEX,
    N_ID,
    N_LIT
} NodeKind;

typedef struct Node Node;
typedef struct NodeList NodeList;
typedef struct Param Param;

struct NodeList {
    Node *node;
    NodeList *next;
};

struct Param {
    char *type;
    char *name;
    bool is_array;
    int line;
    int column;
    Param *next;
};

struct Node {
    NodeKind kind;
    char *a;
    char *b;
    Node *left;
    Node *right;
    Node *third;
    NodeList *items;
    Param *params;
    int token_start;
    int token_end;
};

typedef struct {
    TokenList tokens;
    int pos;
    int failed;
} Parser;

Node *parse_program(Parser *parser);

#endif
