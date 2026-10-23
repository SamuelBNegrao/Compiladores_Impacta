#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LEXER_SEM_MAIN
#include "../../lexer/lexer_v2.c"
#define PARSER_SEMANTIC_LIBRARY
#include "../../lexer/parser.c"

typedef struct {
    char base[20];
    bool array;
    int size;
} SemType;

typedef enum { SYMBOL_VARIABLE, SYMBOL_PARAMETER, SYMBOL_FUNCTION } SymbolKind;

typedef struct Symbol Symbol;
typedef struct Scope Scope;

struct Symbol {
    char *name;
    SymbolKind kind;
    SemType type;
    SemType *parameters;
    int parameter_count;
    int line;
    int column;
    Symbol *next;
};

struct Scope {
    Symbol *symbols;
    Scope *parent;
};

typedef struct {
    char *code;
    char *message;
    int line;
    int column;
    int sequence;
} Diagnostic;

typedef struct {
    const char *source;
    int source_size;
    TokenList *tokens;
    int *line_offsets;
    int line_count;
    Scope *scope;
    Symbol *function;
    int loop_depth;
    Diagnostic *diagnostics;
    int diagnostic_count;
    int diagnostic_capacity;
    int next_sequence;
} Semantic;

static char *copy_string(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = (char *)malloc(length);
    if (copy != NULL) memcpy(copy, text, length);
    return copy;
}

static char *format_string(const char *format, ...) {
    va_list args;
    va_list copy;
    int length;
    char *text;
    va_start(args, format);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return copy_string("");
    }
    text = (char *)malloc((size_t)length + 1);
    if (text != NULL) vsnprintf(text, (size_t)length + 1, format, args);
    va_end(args);
    return text;
}

static SemType make_type(const char *base, bool array, int size) {
    SemType type;
    snprintf(type.base, sizeof(type.base), "%s", base);
    type.array = array;
    type.size = size;
    return type;
}

static SemType error_type(void) { return make_type("erro", false, 0); }
static SemType void_type(void) { return make_type("void", false, 0); }
static SemType bool_type(void) { return make_type("bool", false, 0); }
static SemType int_type(void) { return make_type("int", false, 0); }
static SemType float_type(void) { return make_type("float", false, 0); }

static bool is_error(SemType type) { return strcmp(type.base, "erro") == 0; }
static bool is_void(SemType type) { return !type.array && strcmp(type.base, "void") == 0; }
static bool is_numeric(SemType type) {
    return !type.array && (strcmp(type.base, "int") == 0 || strcmp(type.base, "float") == 0);
}

static char *type_name(SemType type) {
    if (type.array) return format_string("%s[]", type.base);
    return copy_string(type.base);
}

static char *type_description(SemType type) {
    if (type.array && type.size > 0) return format_string("%s[%d]", type.base, type.size);
    return type_name(type);
}

static bool assignable(SemType destination, SemType source) {
    if (is_error(destination) || is_error(source)) return true;
    if (strcmp(destination.base, "void") == 0 || strcmp(destination.base, "string") == 0 ||
        strcmp(source.base, "void") == 0) return false;
    if (destination.array || source.array) {
        return destination.array && source.array && strcmp(destination.base, source.base) == 0;
    }
    return strcmp(destination.base, source.base) == 0 ||
           (strcmp(source.base, "int") == 0 && strcmp(destination.base, "float") == 0);
}

static void open_scope(Semantic *semantic) {
    Scope *scope = (Scope *)calloc(1, sizeof(Scope));
    if (scope == NULL) return;
    scope->parent = semantic->scope;
    semantic->scope = scope;
}

static void close_scope(Semantic *semantic) {
    if (semantic->scope != NULL) semantic->scope = semantic->scope->parent;
}

static Symbol *lookup_symbol(Semantic *semantic, const char *name) {
    for (Scope *scope = semantic->scope; scope != NULL; scope = scope->parent) {
        for (Symbol *symbol = scope->symbols; symbol != NULL; symbol = symbol->next) {
            if (strcmp(symbol->name, name) == 0) return symbol;
        }
    }
    return NULL;
}

static void add_diagnostic(Semantic *semantic, const char *code, int line, int column,
                          const char *format, ...) {
    va_list args;
    va_list copy;
    int length;
    char *message;
    if (semantic->diagnostic_count == semantic->diagnostic_capacity) {
        int capacity = semantic->diagnostic_capacity == 0 ? 16 : semantic->diagnostic_capacity * 2;
        Diagnostic *items = (Diagnostic *)realloc(semantic->diagnostics,
                                                   (size_t)capacity * sizeof(Diagnostic));
        if (items == NULL) return;
        semantic->diagnostics = items;
        semantic->diagnostic_capacity = capacity;
    }
    va_start(args, format);
    va_copy(copy, args);
    length = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (length < 0) {
        va_end(args);
        return;
    }
    message = (char *)malloc((size_t)length + 1);
    if (message == NULL) {
        va_end(args);
        return;
    }
    vsnprintf(message, (size_t)length + 1, format, args);
    va_end(args);
    Diagnostic *diagnostic = &semantic->diagnostics[semantic->diagnostic_count++];
    diagnostic->code = copy_string(code);
    diagnostic->message = message;
    diagnostic->line = line;
    diagnostic->column = column;
    diagnostic->sequence = semantic->next_sequence++;
}

static Token *node_token(Semantic *semantic, const Node *node) {
    if (node == NULL || node->token_start < 0 || node->token_start >= semantic->tokens->quantidade)
        return NULL;
    return &semantic->tokens->itens[node->token_start];
}

static void node_position(Semantic *semantic, const Node *node, int *line, int *column) {
    Token *token = node_token(semantic, node);
    *line = token != NULL ? token->linha : 1;
    *column = token != NULL ? token->coluna : 1;
}

static void name_position(Semantic *semantic, const Node *node, int *line, int *column) {
    int index = node != NULL ? node->token_start + 1 : -1;
    if (index >= 0 && index < semantic->tokens->quantidade) {
        *line = semantic->tokens->itens[index].linha;
        *column = semantic->tokens->itens[index].coluna;
    } else {
        node_position(semantic, node, line, column);
    }
}

static char *node_text(Semantic *semantic, const Node *node) {
    Token *first;
    Token *last;
    size_t begin;
    size_t end;
    size_t length;
    char *text;
    if (node == NULL) return copy_string("");
    if (node->kind == N_ID && node->a != NULL) return copy_string(node->a);
    if (node->kind == N_LIT && node->b != NULL) return copy_string(node->b);
    if (node->kind == N_BINARY) {
        char *left_text = node_text(semantic, node->left);
        char *right_text = node_text(semantic, node->right);
        char *result = format_string("%s %s %s", left_text, node->a != NULL ? node->a : "", right_text);
        free(left_text);
        free(right_text);
        return result;
    }
    if (node->kind == N_UNARY) {
        char *operand_text = node_text(semantic, node->left);
        char *result = format_string("%s%s", node->a != NULL ? node->a : "", operand_text);
        free(operand_text);
        return result;
    }
    if (node->kind == N_ASSIGN) {
        char *left_text = node_text(semantic, node->left);
        char *right_text = node_text(semantic, node->right);
        char *result = format_string("%s = %s", left_text, right_text);
        free(left_text);
        free(right_text);
        return result;
    }
    if (node->kind == N_INDEX) {
        char *base_text = node_text(semantic, node->left);
        char *index_text = node_text(semantic, node->right);
        char *result = format_string("%s[%s]", base_text, index_text);
        free(base_text);
        free(index_text);
        return result;
    }
    if (node->kind == N_CALL) {
        char *callee_text = node_text(semantic, node->left);
        size_t count = 0;
        for (NodeList *item = node->items; item != NULL; item = item->next) count++;
        char *result = NULL;
        if (count == 0) {
            result = format_string("%s()", callee_text);
        } else {
            size_t index = 0;
            char *args = copy_string("");
            for (NodeList *item = node->items; item != NULL; item = item->next) {
                char *text_value = node_text(semantic, item->node);
                char *next_args = index == 0 ? format_string("%s", text_value)
                                            : format_string("%s, %s", args, text_value);
                free(args);
                free(text_value);
                args = next_args;
                index++;
            }
            result = format_string("%s(%s)", callee_text, args);
            free(args);
        }
        free(callee_text);
        return result;
    }
    if (node->token_start < 0 || node->token_end <= node->token_start ||
        node->token_end > semantic->tokens->quantidade) return copy_string("");
    first = &semantic->tokens->itens[node->token_start];
    last = &semantic->tokens->itens[node->token_end - 1];
    if (first->linha < 1 || first->linha > semantic->line_count ||
        last->linha < 1 || last->linha > semantic->line_count) return copy_string("");
    begin = (size_t)semantic->line_offsets[first->linha - 1] + (size_t)(first->coluna - 1);
    end = (size_t)semantic->line_offsets[last->linha - 1] + (size_t)(last->coluna - 1) +
          strlen(last->lexema);
    if (begin > (size_t)semantic->source_size || end > (size_t)semantic->source_size || end < begin)
        return copy_string("");
    length = end - begin;
    text = (char *)malloc(length + 1);
    if (text != NULL) {
        memcpy(text, semantic->source + begin, length);
        text[length] = '\0';
    }
    return text != NULL ? text : copy_string("");
}

static bool node_kind_is(const Node *node, NodeKind kind) {
    return node != NULL && node->kind == kind;
}

static Node *node_left(const Node *node) {
    return node != NULL ? node->left : NULL;
}

static Node *node_right(const Node *node) {
    return node != NULL ? node->right : NULL;
}

static Node *node_third(const Node *node) {
    return node != NULL ? node->third : NULL;
}

static NodeList *node_items(const Node *node) {
    return node != NULL ? node->items : NULL;
}

static Param *node_params(const Node *node) {
    return node != NULL ? node->params : NULL;
}

static const char *node_name(const Node *node) {
    if (node == NULL) return "";
    if (node->kind == N_ID && node->a != NULL) return node->a;
    if (node->b != NULL) return node->b;
    return "";
}


static bool declare_symbol(Semantic *semantic, Symbol *symbol) {
    Symbol *previous;
    char *description;
    if (semantic->scope == NULL || symbol == NULL) return false;
    for (previous = semantic->scope->symbols; previous != NULL; previous = previous->next) {
        if (strcmp(previous->name, symbol->name) == 0) {
            description = previous->kind == SYMBOL_FUNCTION
                ? format_string("função com retorno %s", previous->type.base)
                : format_string("tipo %s", type_description(previous->type));
            add_diagnostic(semantic, "SEM002", symbol->line, symbol->column,
                           "“%s” já declarado neste escopo; declaração anterior na linha %d, coluna %d (%s).",
                           symbol->name, previous->line, previous->column, description);
            return false;
        }
    }
    symbol->next = semantic->scope->symbols;
    semantic->scope->symbols = symbol;
    return true;
}

static Symbol *new_symbol(const char *name, SymbolKind kind, SemType type, int line, int column) {
    Symbol *symbol = (Symbol *)calloc(1, sizeof(Symbol));
    if (symbol == NULL) return NULL;
    symbol->name = copy_string(name);
    symbol->kind = kind;
    symbol->type = type;
    symbol->line = line;
    symbol->column = column;
    return symbol;
}

static SemType semantic_expr(Semantic *semantic, Node *node);
static SemType semantic_value(Semantic *semantic, Node *node, const char *context);
static SemType semantic_target(Semantic *semantic, Node *node, const char *context);
static bool semantic_stmt(Semantic *semantic, Node *node, char **fall_reason);

static SemType semantic_index(Semantic *semantic, Node *node) {
    Node *base = node_left(node);
    Node *index = node_right(node);
    SemType base_type = semantic_expr(semantic, base);
    SemType index_type = semantic_value(semantic, index, "índice de vetor");
    char *name = node_text(semantic, base);
    int line;
    int column;
    if (!is_error(base_type) && !base_type.array) {
        node_position(semantic, base, &line, &column);
        add_diagnostic(semantic, "SEM016", line, column,
                       "“%s” é do tipo %s e não pode ser indexado; apenas vetores aceitam índice.",
                       name, type_name(base_type));
        base_type = error_type();
    }
    if (!is_error(index_type) && strcmp(index_type.base, "int") != 0) {
        node_position(semantic, index, &line, &column);
        add_diagnostic(semantic, "SEM006", line, column,
                       "Índice do vetor “%s” deve ser int; recebeu %s (expressão “%s”).",
                       name, type_name(index_type), node_text(semantic, index));
    }
    free(name);
    return is_error(base_type) ? error_type() : make_type(base_type.base, false, 0);
}

static SemType semantic_call(Semantic *semantic, Node *node) {
    Node *callee = node_left(node);
    NodeList *arguments = node_items(node);
    int actual_count = 0;
    int line;
    int column;
    SemType *actual_types;
    Symbol *symbol;
    if (callee == NULL || !node_kind_is(callee, N_ID)) {
        if (callee != NULL) semantic_expr(semantic, callee);
        node_position(semantic, callee, &line, &column);
        add_diagnostic(semantic, "SEM018", line, column,
                       "A expressão “%s” não é uma função e não pode ser chamada.",
                       node_text(semantic, callee));
        for (NodeList *item = arguments; item != NULL; item = item->next)
            semantic_value(semantic, item->node, "argumento de chamada");
        return error_type();
    }
    for (NodeList *item = arguments; item != NULL; item = item->next) actual_count++;
    actual_types = actual_count > 0 ? (SemType *)calloc((size_t)actual_count, sizeof(SemType)) : NULL;
    int index = 0;
    for (NodeList *item = arguments; item != NULL; item = item->next)
        actual_types[index++] = semantic_value(semantic, item->node, "argumento de chamada");

    const char *name = node_name(callee);
    if (strcmp(name, "print") == 0 || strcmp(name, "read") == 0) {
        index = 0;
        for (NodeList *item = arguments; item != NULL; item = item->next) {
            index++;
            if (strcmp(name, "print") == 0) {
                SemType type = actual_types[index - 1];
                if (type.array) {
                    node_position(semantic, item->node, &line, &column);
                    add_diagnostic(semantic, "SEM008", line, column,
                                   "Argumento %d de “print”: não é possível imprimir um vetor inteiro (expressão “%s”).",
                                   index, node_text(semantic, item->node));
                }
            } else {
                semantic_target(semantic, item->node, "leitura");
            }
        }
        free(actual_types);
        return void_type();
    }

    symbol = lookup_symbol(semantic, name);
    if (symbol == NULL) {
        node_position(semantic, callee, &line, &column);
        add_diagnostic(semantic, "SEM001", line, column,
                       "Identificador “%s” não declarado neste escopo.", name);
        free(actual_types);
        return error_type();
    }
    if (symbol->kind != SYMBOL_FUNCTION) {
        node_position(semantic, callee, &line, &column);
        add_diagnostic(semantic, "SEM018", line, column,
                       "“%s” não é uma função e não pode ser chamado.", name);
        free(actual_types);
        return error_type();
    }
    if (actual_count != symbol->parameter_count) {
        node_position(semantic, node, &line, &column);
        add_diagnostic(semantic, "SEM007", line, column,
                       "“%s” espera %d argumento%s, mas recebeu %d.", name,
                       symbol->parameter_count, symbol->parameter_count == 1 ? "" : "s", actual_count);
    } else {
        index = 0;
        for (NodeList *item = arguments; item != NULL; item = item->next) {
            SemType expected = symbol->parameters[index];
            SemType received = actual_types[index];
            if (!assignable(expected, received)) {
                node_position(semantic, item->node, &line, &column);
                add_diagnostic(semantic, "SEM008", line, column,
                               "Argumento %d de “%s”: esperado %s, recebido %s (expressão “%s”).",
                               index + 1, name, type_name(expected), type_name(received),
                               node_text(semantic, item->node));
            }
            index++;
        }
    }
    free(actual_types);
    return symbol->type;
}

static SemType semantic_value(Semantic *semantic, Node *node, const char *context) {
    SemType type = semantic_expr(semantic, node);
    if (is_void(type)) {
        int line;
        int column;
        const char *name = node != NULL && node->kind == N_CALL && node->left != NULL &&
                           node->left->kind == N_ID ? node->left->a : NULL;
        char *expression = name == NULL ? node_text(semantic, node) : NULL;
        node_position(semantic, node, &line, &column);
        add_diagnostic(semantic, "SEM012", line, column,
                       "Função “%s” não produz valor (retorno void) e não pode ser usada como %s.",
                       name != NULL ? name : expression, context);
        return error_type();
    }
    return type;
}

static SemType semantic_expr(Semantic *semantic, Node *node) {
    int line;
    int column;
    if (node == NULL) return error_type();
    switch (node->kind) {
        case N_LIT:
            if (strcmp(node->a, "int") == 0) return int_type();
            if (strcmp(node->a, "real") == 0) return float_type();
            if (strcmp(node->a, "bool") == 0) return bool_type();
            return make_type(node->a, false, 0);
        case N_ID: {
            Symbol *symbol = lookup_symbol(semantic, node->a);
            if (symbol == NULL) {
                node_position(semantic, node, &line, &column);
                add_diagnostic(semantic, "SEM001", line, column,
                               "Identificador “%s” não declarado neste escopo.", node->a);
                return error_type();
            }
            if (symbol->kind == SYMBOL_FUNCTION) {
                node_position(semantic, node, &line, &column);
                add_diagnostic(semantic, "SEM017", line, column,
                               "“%s” é uma função e não pode ser usada como variável.", node->a);
                return error_type();
            }
            return symbol->type;
        }
        case N_INDEX:
            return semantic_index(semantic, node);
        case N_CALL:
            return semantic_call(semantic, node);
        case N_ASSIGN: {
            SemType value_type = semantic_value(semantic, node->right, "expressão de atribuição");
            SemType target_type = semantic_target(semantic, node->left, "atribuição");
            if (!is_error(target_type) && !is_error(value_type) && !assignable(target_type, value_type)) {
                node_position(semantic, node->right, &line, &column);
                add_diagnostic(semantic, "SEM003", line, column,
                               "Não é possível atribuir %s a %s sem conversão permitida (destino “%s”; expressão “%s”).",
                               type_name(value_type), type_description(target_type),
                               node_text(semantic, node->left), node_text(semantic, node->right));
            }
            return target_type;
        }
        case N_UNARY: {
            SemType operand = semantic_value(semantic, node->left, "operando");
            if (is_error(operand)) return strcmp(node->a, "!") == 0 ? bool_type() : error_type();
            if (strcmp(node->a, "!") == 0) {
                if (strcmp(operand.base, "bool") != 0 || operand.array) {
                    node_position(semantic, node, &line, &column);
                    add_diagnostic(semantic, "SEM004", line, column,
                                   "Operador “!” exige operando bool; recebeu %s (expressão “%s”).",
                                   type_name(operand), node_text(semantic, node));
                    return error_type();
                }
                return bool_type();
            }
            if (!is_numeric(operand)) {
                node_position(semantic, node, &line, &column);
                add_diagnostic(semantic, "SEM004", line, column,
                               "Operador “%s” exige operando numérico (int ou float); recebeu %s (expressão “%s”).",
                               node->a, type_name(operand), node_text(semantic, node));
                return error_type();
            }
            return operand;
        }
        case N_BINARY: {
            SemType left = semantic_value(semantic, node->left, "operando");
            SemType right = semantic_value(semantic, node->right, "operando");
            const char *op = node->a;
            bool arithmetic = strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
                              strcmp(op, "*") == 0 || strcmp(op, "/") == 0 || strcmp(op, "%") == 0;
            bool valid;
            const char *expected;
            SemType result = arithmetic ? error_type() : bool_type();
            if (is_error(left) || is_error(right)) return result;
            if (arithmetic) {
                valid = strcmp(op, "%") == 0
                    ? strcmp(left.base, "int") == 0 && strcmp(right.base, "int") == 0 && !left.array && !right.array
                    : is_numeric(left) && is_numeric(right);
                expected = strcmp(op, "%") == 0 ? "int" : "numéricos (int ou float)";
            } else if (strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
                       strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0) {
                valid = (is_numeric(left) && is_numeric(right)) ||
                        (!left.array && !right.array && strcmp(left.base, "char") == 0 && strcmp(right.base, "char") == 0);
                expected = "numéricos (int ou float) ou ambos char";
            } else if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
                valid = (is_numeric(left) && is_numeric(right)) ||
                        (!left.array && !right.array && strcmp(left.base, right.base) == 0 &&
                         (strcmp(left.base, "bool") == 0 || strcmp(left.base, "char") == 0));
                expected = "de tipos compatíveis";
            } else {
                valid = strcmp(left.base, "bool") == 0 && strcmp(right.base, "bool") == 0 &&
                        !left.array && !right.array;
                expected = "bool";
            }
            if (!valid) {
                node_position(semantic, node, &line, &column);
                add_diagnostic(semantic, "SEM004", line, column,
                               "Operador “%s” exige operandos %s; recebeu %s e %s (expressão “%s”).",
                               op, expected, type_name(left), type_name(right), node_text(semantic, node));
                return result;
            }
            if (arithmetic) return strcmp(left.base, "float") == 0 || strcmp(right.base, "float") == 0
                ? float_type() : int_type();
            return bool_type();
        }
        case N_EXPRSTMT:
            return semantic_expr(semantic, node->left);
        default:
            return error_type();
    }
}

static SemType semantic_target(Semantic *semantic, Node *node, const char *context) {
    int line;
    int column;
    if (node != NULL && node->kind == N_INDEX) return semantic_index(semantic, node);
    if (node != NULL && node->kind == N_ID) {
        Symbol *symbol = lookup_symbol(semantic, node->a);
        if (symbol == NULL) {
            node_position(semantic, node, &line, &column);
            add_diagnostic(semantic, "SEM001", line, column,
                           "Identificador “%s” não declarado neste escopo.", node->a);
            return error_type();
        }
        if (symbol->kind == SYMBOL_FUNCTION) {
            node_position(semantic, node, &line, &column);
            add_diagnostic(semantic, "SEM013", line, column,
                           "Destino de %s não é atribuível; a função “%s” não designa uma variável ou elemento de vetor.",
                           context, node->a);
            return error_type();
        }
        if (symbol->type.array) {
            node_position(semantic, node, &line, &column);
            add_diagnostic(semantic, "SEM013", line, column,
                           "Destino de %s não é atribuível; o vetor “%s” não pode ser tratado como um todo; use um elemento (por exemplo, “%s[0]”).",
                           context, node->a, node->a);
            return error_type();
        }
        return symbol->type;
    }
    SemType type = semantic_expr(semantic, node);
    const char *description = "a expressão";
    if (node != NULL && node->kind == N_LIT) {
        if (strcmp(node->a, "int") == 0) description = "o literal inteiro";
        else if (strcmp(node->a, "real") == 0) description = "o literal real";
        else if (strcmp(node->a, "char") == 0) description = "o literal de caractere";
        else if (strcmp(node->a, "string") == 0) description = "o literal de cadeia";
        else if (strcmp(node->a, "bool") == 0) description = "o literal booleano";
    }
    node_position(semantic, node, &line, &column);
    add_diagnostic(semantic, "SEM013", line, column,
                   "Destino de %s não é atribuível; %s “%s” não designa uma variável ou elemento de vetor.",
                   context, description, node_text(semantic, node));
    (void)type;
    return error_type();
}

static long constant_integer(Node *node, bool *valid) {
    char *end;
    long value;
    if (node == NULL) {
        *valid = false;
        return 0;
    }
    if (node->kind == N_LIT && strcmp(node->a, "int") == 0) {
        value = strtol(node->b, &end, 10);
        *valid = *end == '\0';
        return value;
    }
    if (node->kind == N_UNARY && strcmp(node->a, "-") == 0) {
        value = constant_integer(node->left, valid);
        return *valid ? -value : 0;
    }
    *valid = false;
    return 0;
}

static void semantic_decl(Semantic *semantic, Node *node) {
    int line;
    int column;
    SemType type = make_type(node->a, false, 0);
    name_position(semantic, node, &line, &column);
    if (strcmp(node->a, "void") == 0) {
        add_diagnostic(semantic, "SEM014", line, column,
                       "A variável “%s” não pode ter tipo void.", node->b);
        type = error_type();
    }
    if (node->left != NULL) {
        semantic_value(semantic, node->left, "tamanho de vetor");
        bool valid;
        long size = constant_integer(node->left, &valid);
        if (!valid || size <= 0) {
            node_position(semantic, node->left, &line, &column);
            add_diagnostic(semantic, "SEM019", line, column,
                           "O tamanho do vetor “%s” deve ser uma constante inteira positiva (expressão “%s”).",
                           node->b, node_text(semantic, node->left));
        } else if (!is_error(type)) {
            type.array = true;
            type.size = size > 2147483647L ? 0 : (int)size;
        }
    }
    if (node->right != NULL) {
        SemType initializer = semantic_value(semantic, node->right, "inicializador de variável");
        if (!assignable(type, initializer)) {
            node_position(semantic, node->right, &line, &column);
            add_diagnostic(semantic, "SEM003", line, column,
                           "Não é possível atribuir %s a %s sem conversão permitida (destino “%s”; expressão “%s”).",
                           type_name(initializer), type_description(type), node->b,
                           node_text(semantic, node->right));
        }
    }
    declare_symbol(semantic, new_symbol(node->b, SYMBOL_VARIABLE, type, line, column));
}

static void semantic_condition(Semantic *semantic, Node *node, const char *keyword) {
    SemType type = semantic_value(semantic, node, "condição");
    int line;
    int column;
    if (!is_error(type) && (strcmp(type.base, "bool") != 0 || type.array)) {
        node_position(semantic, node, &line, &column);
        add_diagnostic(semantic, "SEM005", line, column,
                       "Condição de %s deve ter tipo bool; recebeu %s (expressão “%s”).",
                       keyword, type_name(type), node_text(semantic, node));
    }
}

static void semantic_return(Semantic *semantic, Node *node) {
    SemType return_type = semantic->function->type;
    if (node->left == NULL) {
        if (strcmp(return_type.base, "void") != 0) {
            int line;
            int column;
            node_position(semantic, node, &line, &column);
            add_diagnostic(semantic, "SEM010", line, column,
                           "A função “%s” deve retornar %s, mas o return não tem valor.",
                           semantic->function->name, type_name(return_type));
        }
        return;
    }
    SemType actual = semantic_value(semantic, node->left, "valor de retorno");
    int line;
    int column;
    if (strcmp(return_type.base, "void") == 0) {
        if (!is_error(actual)) {
            node_position(semantic, node->left, &line, &column);
            add_diagnostic(semantic, "SEM010", line, column,
                           "A função void “%s” não pode retornar valor (expressão “%s” do tipo %s).",
                           semantic->function->name, node_text(semantic, node->left), type_name(actual));
        }
    } else if (!assignable(return_type, actual)) {
        node_position(semantic, node->left, &line, &column);
        add_diagnostic(semantic, "SEM009", line, column,
                       "Retorno %s incompatível com o tipo %s da função “%s”; conversão implícita de %s para %s não permitida.",
                       type_name(actual), type_name(return_type), semantic->function->name,
                       type_name(actual), type_name(return_type));
    }
}

static bool semantic_branch(Semantic *semantic, Node *node, char **fall_reason) {
    if (node != NULL && node->kind == N_BLOCK) return semantic_stmt(semantic, node, fall_reason);
    open_scope(semantic);
    bool returns = semantic_stmt(semantic, node, fall_reason);
    close_scope(semantic);
    return returns;
}

static bool semantic_if(Semantic *semantic, Node *node, char **fall_reason) {
    Node *condition = node_left(node);
    Node *then_branch = node_right(node);
    Node *else_branch = node_third(node);
    char *condition_text;
    char *then_reason = NULL;
    char *else_reason = NULL;
    bool then_returns;
    bool else_returns;
    semantic_condition(semantic, condition, "if");
    condition_text = node_text(semantic, condition);
    then_returns = semantic_branch(semantic, then_branch, &then_reason);
    if (else_branch == NULL) {
        *fall_reason = format_string("o ramo em que “%s” é falso alcança o fim do corpo", condition_text);
        free(condition_text);
        return false;
    }
    else_returns = semantic_branch(semantic, else_branch, &else_reason);
    if (then_returns && else_returns) {
        free(condition_text);
        return true;
    }
    if (!then_returns) {
        *fall_reason = then_reason != NULL && then_reason[0] != '\0' ? then_reason :
            format_string("o ramo em que “%s” é verdadeiro alcança o fim do corpo", condition_text);
    } else {
        *fall_reason = else_reason != NULL && else_reason[0] != '\0' ? else_reason :
            format_string("o ramo em que “%s” é falso alcança o fim do corpo", condition_text);
    }
    free(condition_text);
    return false;
}

static bool semantic_stmt(Semantic *semantic, Node *node, char **fall_reason) {
    if (fall_reason != NULL) *fall_reason = copy_string("");
    if (node == NULL) return false;
    switch (node->kind) {
        case N_BLOCK: {
            open_scope(semantic);
            bool returns = false;
            char *reason = copy_string("");
            for (NodeList *item = node->items; item != NULL; item = item->next) {
                char *next_reason = NULL;
                if (semantic_stmt(semantic, item->node, &next_reason)) returns = true;
                else if (next_reason != NULL && next_reason[0] != '\0') reason = next_reason;
            }
            close_scope(semantic);
            if (fall_reason != NULL) *fall_reason = reason;
            return returns;
        }
        case N_VARDECL:
            semantic_decl(semantic, node);
            return false;
        case N_IF:
            return semantic_if(semantic, node, fall_reason);
        case N_WHILE:
            semantic_condition(semantic, node->left, "while");
            semantic->loop_depth++;
            semantic_branch(semantic, node->right, NULL);
            semantic->loop_depth--;
            return false;
        case N_FOR:
            if (node->left != NULL) semantic_expr(semantic, node->left);
            if (node->right != NULL) semantic_condition(semantic, node->right, "for");
            semantic->loop_depth++;
            if (node->items != NULL) semantic_branch(semantic, node->items->node, NULL);
            semantic->loop_depth--;
            if (node->third != NULL) semantic_expr(semantic, node->third);
            return false;
        case N_BREAK:
        case N_CONTINUE:
            if (semantic->loop_depth == 0) {
                int line;
                int column;
                node_position(semantic, node, &line, &column);
                add_diagnostic(semantic, node->kind == N_BREAK ? "SEM020" : "SEM021",
                               line, column, "%s só pode ocorrer dentro de while ou for.",
                               node->kind == N_BREAK ? "break" : "continue");
            }
            return false;
        case N_RETURN:
            semantic_return(semantic, node);
            return true;
        case N_EXPRSTMT:
            semantic_expr(semantic, node->left);
            return false;
        default:
            return false;
    }
}

static int compare_diagnostics(const void *left_pointer, const void *right_pointer) {
    const Diagnostic *left = (const Diagnostic *)left_pointer;
    const Diagnostic *right = (const Diagnostic *)right_pointer;
    if (left->line != right->line) return left->line < right->line ? -1 : 1;
    if (left->column != right->column) return left->column < right->column ? -1 : 1;
    return left->sequence - right->sequence;
}

static void collect_function_signature(Semantic *semantic, Node *node) {
    Param *parameter;
    int line;
    int column;
    int count = 0;
    for (parameter = node_params(node); parameter != NULL; parameter = parameter->next) count++;
    name_position(semantic, node, &line, &column);
    Symbol *symbol = new_symbol(node_name(node), SYMBOL_FUNCTION, make_type(node->a, false, 0), line, column);
    if (symbol == NULL) return;
    symbol->parameter_count = count;
    if (count > 0) symbol->parameters = (SemType *)calloc((size_t)count, sizeof(SemType));
    int index = 0;
    for (parameter = node_params(node); parameter != NULL; parameter = parameter->next) {
        SemType type = make_type(parameter->type, parameter->is_array != 0, 0);
        if (strcmp(parameter->type, "void") == 0) {
            add_diagnostic(semantic, "SEM014", parameter->line, parameter->column,
                           "O parâmetro “%s” não pode ter tipo void.", parameter->name);
            type = error_type();
        }
        if (symbol->parameters != NULL) symbol->parameters[index] = type;
        index++;
    }
    declare_symbol(semantic, symbol);
}

static void analyze_function(Semantic *semantic, Node *node) {
    Symbol *symbol = lookup_symbol(semantic, node_name(node));
    if (symbol == NULL || symbol->kind != SYMBOL_FUNCTION) return;
    semantic->function = symbol;
    open_scope(semantic);
    Param *parameter = node_params(node);
    int index = 0;
    while (parameter != NULL) {
        SemType type = symbol->parameters != NULL ? symbol->parameters[index] : error_type();
        declare_symbol(semantic, new_symbol(parameter->name, SYMBOL_PARAMETER, type,
                                             parameter->line, parameter->column));
        parameter = parameter->next;
        index++;
    }
    char *fall_reason = NULL;
    bool returns = true;
    if (node_left(node) != NULL) {
        returns = false;
        for (NodeList *item = node_items(node_left(node)); item != NULL; item = item->next) {
            char *next_reason = NULL;
            if (semantic_stmt(semantic, item->node, &next_reason)) returns = true;
            else if (next_reason != NULL && next_reason[0] != '\0') fall_reason = next_reason;
        }
    }
    if (!returns && strcmp(symbol->type.base, "void") != 0) {
        int line;
        int column;
        node_position(semantic, node, &line, &column);
        const char *reason = fall_reason != NULL && fall_reason[0] != '\0'
            ? fall_reason : "o corpo alcança o fim sem executar um return";
        add_diagnostic(semantic, "SEM011", line, column,
                       "A função “%s” pode terminar sem retornar %s; %s.",
                       symbol->name, type_name(symbol->type), reason);
    }
    close_scope(semantic);
    semantic->function = NULL;
}

static void analyze_program(Semantic *semantic, Node *program) {
    open_scope(semantic);
    for (NodeList *item = program->items; item != NULL; item = item->next) {
        if (item->node->kind == N_FUNCTION) collect_function_signature(semantic, item->node);
    }
    for (NodeList *item = program->items; item != NULL; item = item->next) {
        Node *node = item->node;
        if (node->kind == N_FUNCTION) {
            analyze_function(semantic, node);
        } else if (node->kind == N_VARDECL) {
            semantic_decl(semantic, node);
        } else {
            int line;
            int column;
            node_position(semantic, node, &line, &column);
            add_diagnostic(semantic, "SEM015", line, column,
                           "Apenas declarações são permitidas no escopo global; comandos devem estar dentro de uma função.");
        }
    }
    close_scope(semantic);
    qsort(semantic->diagnostics, (size_t)semantic->diagnostic_count,
          sizeof(Diagnostic), compare_diagnostics);
}

static int *make_line_offsets(const char *source, int size, int *line_count) {
    int count = 1;
    for (int index = 0; index < size; index++) if (source[index] == '\n') count++;
    int *offsets = (int *)malloc((size_t)count * sizeof(int));
    if (offsets == NULL) return NULL;
    offsets[0] = 0;
    int line = 1;
    for (int index = 0; index < size; index++) {
        if (source[index] == '\n' && line < count) offsets[line++] = index + 1;
    }
    *line_count = count;
    return offsets;
}

static void print_diagnostics(Semantic *semantic) {
    for (int index = 0; index < semantic->diagnostic_count; index++) {
        Diagnostic *diagnostic = &semantic->diagnostics[index];
        printf("%s — linha %d, coluna %d: %s\n", diagnostic->code,
               diagnostic->line, diagnostic->column, diagnostic->message);
    }
    if (semantic->diagnostic_count == 0) {
        printf("Análise semântica concluída: 0 erros; programa aceito.");
    } else {
        printf("Análise semântica concluída: %d erro%s; programa rejeitado.",
               semantic->diagnostic_count, semantic->diagnostic_count == 1 ? "" : "s");
    }
}

typedef struct {
    char original[256];
    char emitted[256];
} IRBinding;

typedef struct {
    IRBinding bindings[128];
    int count;
} IRScope;

typedef struct {
    int next_temp;
    int next_label;
    int next_shadow;
    IRScope scopes[64];
    int scope_count;
    char *break_labels[64];
    char *continue_labels[64];
    int loop_count;
} IRGenerator;

static char *ir_temp(IRGenerator *generator) {
    return format_string("t%d", ++generator->next_temp);
}

static char *ir_label(IRGenerator *generator, const char *base) {
    return format_string("L_%s_%d", base, ++generator->next_label);
}

static void ir_emit(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("  ");
    vprintf(format, args);
    putchar('\n');
    va_end(args);
}

static void ir_emit_label(const char *label) {
    printf("%s:\n", label);
}

static void ir_push_scope(IRGenerator *generator) {
    if (generator->scope_count < (int)(sizeof(generator->scopes) / sizeof(generator->scopes[0]))) {
        memset(&generator->scopes[generator->scope_count], 0, sizeof(IRScope));
        generator->scope_count++;
    }
}

static void ir_pop_scope(IRGenerator *generator) {
    if (generator->scope_count > 0) generator->scope_count--;
}

static char *ir_declare(IRGenerator *generator, const char *name) {
    bool shadows = false;
    for (int scope = generator->scope_count - 2; scope >= 0; scope--) {
        for (int index = generator->scopes[scope].count - 1; index >= 0; index--) {
            if (strcmp(generator->scopes[scope].bindings[index].original, name) == 0) {
                shadows = true;
                break;
            }
        }
        if (shadows) break;
    }
    char *emitted = shadows
        ? format_string("%s#%d", name, ++generator->next_shadow)
        : copy_string(name);
    if (generator->scope_count > 0) {
        IRScope *scope = &generator->scopes[generator->scope_count - 1];
        if (scope->count < (int)(sizeof(scope->bindings) / sizeof(scope->bindings[0]))) {
            IRBinding *binding = &scope->bindings[scope->count++];
            snprintf(binding->original, sizeof(binding->original), "%s", name);
            snprintf(binding->emitted, sizeof(binding->emitted), "%s", emitted);
        }
    }
    return emitted;
}

static char *ir_lookup(IRGenerator *generator, const char *name) {
    for (int scope = generator->scope_count - 1; scope >= 0; scope--) {
        for (int index = generator->scopes[scope].count - 1; index >= 0; index--) {
            IRBinding *binding = &generator->scopes[scope].bindings[index];
            if (strcmp(binding->original, name) == 0) return copy_string(binding->emitted);
        }
    }
    return copy_string(name);
}

static char *ir_expr(IRGenerator *generator, Node *node);
static char *ir_lvalue(IRGenerator *generator, Node *node);
static void ir_stmt(IRGenerator *generator, Node *node);

static char *ir_lvalue(IRGenerator *generator, Node *node) {
    if (node == NULL) return copy_string("");
    if (node->kind == N_ID) return ir_lookup(generator, node->a);
    if (node->kind == N_INDEX) {
        char *base = ir_expr(generator, node->left);
        char *index = ir_expr(generator, node->right);
        char *scaled = ir_temp(generator);
        ir_emit("%s = %s * 1", scaled, index);
        char *address = format_string("%s[%s]", base, scaled);
        free(base);
        free(index);
        free(scaled);
        return address;
    }
    return copy_string("");
}

static void ir_builtin(IRGenerator *generator, Node *call) {
    const char *name = node_name(call->left);
    for (NodeList *item = call->items; item != NULL; item = item->next) {
        if (strcmp(name, "read") == 0) {
            char *target = ir_lvalue(generator, item->node);
            ir_emit("read %s", target);
            free(target);
        } else {
            char *value = ir_expr(generator, item->node);
            ir_emit("print %s", value);
            free(value);
        }
    }
}

static char *ir_expr(IRGenerator *generator, Node *node) {
    if (node == NULL) return copy_string("");
    if (node->kind == N_LIT) {
        if (strcmp(node->a, "char") == 0)
            return format_string("'%s'", node->b != NULL ? node->b : "");
        if (strcmp(node->a, "string") == 0)
            return format_string("\"%s\"", node->b != NULL ? node->b : "");
        return copy_string(node->b != NULL ? node->b : "");
    }
    if (node->kind == N_ID) return ir_lookup(generator, node->a);
    if (node->kind == N_INDEX) {
        char *address = ir_lvalue(generator, node);
        char *result = ir_temp(generator);
        ir_emit("%s = load %s", result, address);
        free(address);
        return result;
    }
    if (node->kind == N_ASSIGN) {
        char *value = ir_expr(generator, node->right);
        char *target = ir_lvalue(generator, node->left);
        if (node->left != NULL && node->left->kind == N_INDEX)
            ir_emit("store %s, %s", target, value);
        else
            ir_emit("%s = %s", target, value);
        free(value);
        return target;
    }
    if (node->kind == N_UNARY) {
        char *operand = ir_expr(generator, node->left);
        char *result = ir_temp(generator);
        ir_emit("%s = %s %s", result, strcmp(node->a, "!") == 0 ? "not" : "neg", operand);
        free(operand);
        return result;
    }
    if (node->kind == N_BINARY) {
        char *left = ir_expr(generator, node->left);
        char *right = ir_expr(generator, node->right);
        char *result = ir_temp(generator);
        const char *op = strcmp(node->a, "&&") == 0 ? "and" :
                         strcmp(node->a, "||") == 0 ? "or" : node->a;
        ir_emit("%s = %s %s %s", result, left, op, right);
        free(left);
        free(right);
        return result;
    }
    if (node->kind == N_CALL) {
        if (node->left != NULL && node->left->kind == N_ID &&
            (strcmp(node->left->a, "print") == 0 || strcmp(node->left->a, "read") == 0)) {
            ir_builtin(generator, node);
            return copy_string("");
        }
        int count = 0;
        for (NodeList *item = node->items; item != NULL; item = item->next) {
            char *argument = ir_expr(generator, item->node);
            ir_emit("param %s", argument);
            free(argument);
            count++;
        }
        char *callee = ir_expr(generator, node->left);
        char *result = ir_temp(generator);
        ir_emit("%s = call %s, %d", result, callee, count);
        free(callee);
        return result;
    }
    return copy_string("");
}

static bool ir_contains_continue(Node *node) {
    if (node == NULL) return false;
    if (node->kind == N_CONTINUE) return true;
    if (node->kind == N_WHILE || node->kind == N_FOR) return false;
    if (node->kind == N_BLOCK) {
        for (NodeList *item = node->items; item != NULL; item = item->next)
            if (ir_contains_continue(item->node)) return true;
    } else if (node->kind == N_IF) {
        return ir_contains_continue(node->right) || ir_contains_continue(node->third);
    }
    return false;
}

static void ir_block(IRGenerator *generator, Node *node, bool new_scope) {
    if (new_scope) ir_push_scope(generator);
    if (node != NULL) {
        for (NodeList *item = node->items; item != NULL; item = item->next)
            ir_stmt(generator, item->node);
    }
    if (new_scope) ir_pop_scope(generator);
}

static void ir_branch(IRGenerator *generator, Node *node) {
    if (node != NULL && node->kind == N_BLOCK) {
        ir_block(generator, node, true);
    } else {
        ir_push_scope(generator);
        ir_stmt(generator, node);
        ir_pop_scope(generator);
    }
}

static void ir_if(IRGenerator *generator, Node *node) {
    char *condition = ir_expr(generator, node->left);
    if (node->third == NULL) {
        char *end = ir_label(generator, "end_if");
        ir_emit("ifFalse %s goto %s", condition, end);
        ir_branch(generator, node->right);
        ir_emit_label(end);
        free(end);
    } else {
        char *alternative = ir_label(generator, "else");
        char *end = ir_label(generator, "end_if");
        ir_emit("ifFalse %s goto %s", condition, alternative);
        ir_branch(generator, node->right);
        ir_emit("goto %s", end);
        ir_emit_label(alternative);
        ir_branch(generator, node->third);
        ir_emit_label(end);
        free(alternative);
        free(end);
    }
    free(condition);
}

static void ir_while(IRGenerator *generator, Node *node) {
    char *test = ir_label(generator, "test");
    char *end = ir_label(generator, "end");
    ir_emit_label(test);
    char *condition = ir_expr(generator, node->left);
    ir_emit("ifFalse %s goto %s", condition, end);
    free(condition);
    generator->break_labels[generator->loop_count] = end;
    generator->continue_labels[generator->loop_count] = test;
    generator->loop_count++;
    ir_branch(generator, node->right);
    generator->loop_count--;
    ir_emit("goto %s", test);
    ir_emit_label(end);
    free(test);
    free(end);
}

static void ir_for(IRGenerator *generator, Node *node) {
    if (node->left != NULL) {
        char *initial = ir_expr(generator, node->left);
        free(initial);
    }
    char *test = ir_label(generator, "test");
    char *end = ir_label(generator, "end");
    bool has_continue = node->items != NULL && ir_contains_continue(node->items->node);
    char *increment = has_continue ? ir_label(generator, "increment") : copy_string(test);
    ir_emit_label(test);
    if (node->right != NULL) {
        char *condition = ir_expr(generator, node->right);
        ir_emit("ifFalse %s goto %s", condition, end);
        free(condition);
    }
    generator->break_labels[generator->loop_count] = end;
    generator->continue_labels[generator->loop_count] = increment;
    generator->loop_count++;
    if (node->items != NULL) ir_branch(generator, node->items->node);
    generator->loop_count--;
    if (has_continue) ir_emit_label(increment);
    if (node->third != NULL) {
        char *step = ir_expr(generator, node->third);
        free(step);
    }
    ir_emit("goto %s", test);
    ir_emit_label(end);
    free(test);
    free(end);
    free(increment);
}

static void ir_stmt(IRGenerator *generator, Node *node) {
    if (node == NULL) return;
    switch (node->kind) {
        case N_BLOCK:
            ir_block(generator, node, true);
            break;
        case N_VARDECL: {
            char *target = ir_declare(generator, node->b);
            if (node->right != NULL) {
                char *value = ir_expr(generator, node->right);
                ir_emit("%s = %s", target, value);
                free(value);
            }
            free(target);
            break;
        }
        case N_EXPRSTMT:
            if (node->left != NULL && node->left->kind == N_CALL &&
                node->left->left != NULL && node->left->left->kind == N_ID &&
                (strcmp(node->left->left->a, "print") == 0 ||
                 strcmp(node->left->left->a, "read") == 0)) {
                ir_builtin(generator, node->left);
            } else {
                char *value = ir_expr(generator, node->left);
                free(value);
            }
            break;
        case N_IF:
            ir_if(generator, node);
            break;
        case N_WHILE:
            ir_while(generator, node);
            break;
        case N_FOR:
            ir_for(generator, node);
            break;
        case N_BREAK:
            if (generator->loop_count > 0)
                ir_emit("goto %s", generator->break_labels[generator->loop_count - 1]);
            break;
        case N_CONTINUE:
            if (generator->loop_count > 0)
                ir_emit("goto %s", generator->continue_labels[generator->loop_count - 1]);
            break;
        case N_RETURN:
            if (node->left == NULL) {
                ir_emit("return");
            } else {
                char *value = ir_expr(generator, node->left);
                ir_emit("return %s", value);
                free(value);
            }
            break;
        default:
            break;
    }
}

static bool generate_ir(Node *program) {
    IRGenerator *generator = (IRGenerator *)calloc(1, sizeof(IRGenerator));
    if (generator == NULL) {
        fprintf(stderr, "Erro: memória insuficiente para inicializar a geração de IR.\n");
        return false;
    }
    ir_push_scope(generator);
    for (NodeList *item = program->items; item != NULL; item = item->next) {
        if (item->node->kind == N_VARDECL) {
            char *name = ir_declare(generator, item->node->b);
            free(name);
        }
    }
    for (NodeList *item = program->items; item != NULL; item = item->next) {
        Node *node = item->node;
        if (node->kind == N_VARDECL) {
            if (node->right != NULL) {
                char *value = ir_expr(generator, node->right);
                printf("global %s = %s\n", node->b, value);
                free(value);
            }
        } else if (node->kind == N_FUNCTION) {
            generator->next_temp = 0;
            generator->next_label = 0;
            generator->next_shadow = 0;
            generator->loop_count = 0;
            ir_push_scope(generator);
            printf("func %s", node->b);
            if (node->params != NULL) {
                putchar('(');
                for (Param *param = node->params; param != NULL; param = param->next) {
                    if (param != node->params) printf(", ");
                    printf("%s", param->name);
                    char *emitted = ir_declare(generator, param->name);
                    free(emitted);
                }
                putchar(')');
            }
            putchar('\n');
            ir_block(generator, node->left, false);
            printf("endfunc\n");
            ir_pop_scope(generator);
        }
    }
    free(generator);
    return true;
}

int main(int argc, char **argv) {
    int source_size;
    char *source;
    const char *source_path = NULL;
    bool ir_mode = false;
    bool check_only = false;
    Parser parser = {0};
    Node *program;
    Semantic semantic = {0};
    for (int index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--ir") == 0 || strcmp(argv[index], "-O0") == 0) {
            ir_mode = true;
        } else if (strcmp(argv[index], "--check") == 0) {
            ir_mode = true;
            check_only = true;
        } else if (strcmp(argv[index], "-O1") == 0) {
            fprintf(stderr, "Erro: otimização -O1 ainda não está implementada.\n");
            return 1;
        } else if (argv[index][0] == '-') {
            fprintf(stderr, "Opção desconhecida: %s\n", argv[index]);
            return 1;
        } else if (source_path == NULL) {
            source_path = argv[index];
        } else {
            fprintf(stderr, "Uso: minic [--ir|-O0|--check] <arquivo.c>\n");
            return 1;
        }
    }
    if (source_path == NULL || (check_only && argc > 3)) {
        fprintf(stderr, "Uso: minic [--ir|-O0|--check] <arquivo.c>\n");
        return 2;
    }
    source = read_file(source_path, &source_size);
    if (source == NULL) {
        fprintf(stderr, "Erro ao abrir arquivo: %s\n", source_path);
        return 2;
    }
    parser.tokens = lexer_tokenizar(source, source_size);
    if (ir_mode && parser.tokens.erros_lexicos > 0) {
        fprintf(stderr, "Erro léxico: linha %d, coluna %d: símbolo '%c' não reconhecido.\n",
                parser.tokens.primeira_linha_erro, parser.tokens.primeira_coluna_erro,
                parser.tokens.primeiro_caractere_invalido);
        token_list_liberar(&parser.tokens);
        free(source);
        return 2;
    }
    program = parse_program(&parser);
    if (parser.failed) {
        token_list_liberar(&parser.tokens);
        free(source);
        return ir_mode ? 3 : 1;
    }
    semantic.source = source;
    semantic.source_size = source_size;
    semantic.tokens = &parser.tokens;
    semantic.line_offsets = make_line_offsets(source, source_size, &semantic.line_count);
    analyze_program(&semantic, program);
    int result = semantic.diagnostic_count == 0 ? 0 : (ir_mode ? 4 : 1);
    if (!ir_mode) {
        print_diagnostics(&semantic);
    } else if (semantic.diagnostic_count > 0) {
        for (int index = 0; index < semantic.diagnostic_count; index++) {
            Diagnostic *diagnostic = &semantic.diagnostics[index];
            fprintf(stderr, "Erro semântico [%s] — linha %d, coluna %d: %s\n",
                    diagnostic->code, diagnostic->line, diagnostic->column,
                    diagnostic->message);
        }
    } else if (check_only) {
        printf("Análise concluída: programa aceito.\n");
    } else {
        int main_count = 0;
        for (NodeList *item = program->items; item != NULL; item = item->next) {
            if (item->node->kind == N_FUNCTION && strcmp(item->node->b, "main") == 0)
                main_count++;
        }
        if (main_count != 1) {
            fprintf(stderr, "Erro semântico: o programa deve definir exatamente uma função main.\n");
            result = 4;
        } else {
            if (!generate_ir(program)) result = 1;
        }
    }
    token_list_liberar(&parser.tokens);
    free(semantic.line_offsets);
    free(semantic.diagnostics);
    free(source);
    return result;
}
