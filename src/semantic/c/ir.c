#include "ir.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ir_copy_string(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = (char *)malloc(length);
    if (copy != NULL) memcpy(copy, text, length);
    return copy;
}

static char *ir_format_string(const char *format, ...) {
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
        return ir_copy_string("");
    }
    text = (char *)malloc((size_t)length + 1);
    if (text != NULL) vsnprintf(text, (size_t)length + 1, format, args);
    va_end(args);
    return text;
}

static const char *ir_node_name(const Node *node) {
    if (node == NULL) return "";
    if (node->kind == N_ID && node->a != NULL) return node->a;
    if (node->b != NULL) return node->b;
    return "";
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

static FILE *ir_output;

static char *ir_temp(IRGenerator *generator) {
    return ir_format_string("t%d", ++generator->next_temp);
}

static char *ir_label(IRGenerator *generator, const char *base) {
    return ir_format_string("L_%s_%d", base, ++generator->next_label);
}

static void ir_emit(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(ir_output != NULL ? ir_output : stdout, "  ");
    vfprintf(ir_output != NULL ? ir_output : stdout, format, args);
    fputc('\n', ir_output != NULL ? ir_output : stdout);
    va_end(args);
}

static void ir_emit_label(const char *label) {
    fprintf(ir_output != NULL ? ir_output : stdout, "%s:\n", label);
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
        ? ir_format_string("%s#%d", name, ++generator->next_shadow)
        : ir_copy_string(name);
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
            if (strcmp(binding->original, name) == 0) return ir_copy_string(binding->emitted);
        }
    }
    return ir_copy_string(name);
}

static char *ir_expr(IRGenerator *generator, Node *node);
static char *ir_lvalue(IRGenerator *generator, Node *node);
static void ir_stmt(IRGenerator *generator, Node *node);

static char *ir_lvalue(IRGenerator *generator, Node *node) {
    if (node == NULL) return ir_copy_string("");
    if (node->kind == N_ID) return ir_lookup(generator, node->a);
    if (node->kind == N_INDEX) {
        char *base = ir_expr(generator, node->left);
        char *index = ir_expr(generator, node->right);
        char *scaled = ir_temp(generator);
        ir_emit("%s = %s * 1", scaled, index);
        char *address = ir_format_string("%s[%s]", base, scaled);
        free(base);
        free(index);
        free(scaled);
        return address;
    }
    return ir_copy_string("");
}

static void ir_builtin(IRGenerator *generator, Node *call) {
    const char *name = ir_node_name(call->left);
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
    if (node == NULL) return ir_copy_string("");
    if (node->kind == N_LIT) {
        if (strcmp(node->a, "char") == 0)
            return ir_format_string("'%s'", node->b != NULL ? node->b : "");
        if (strcmp(node->a, "string") == 0)
            return ir_format_string("\"%s\"", node->b != NULL ? node->b : "");
        return ir_copy_string(node->b != NULL ? node->b : "");
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
            return ir_copy_string("");
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
    return ir_copy_string("");
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
    char *increment = has_continue ? ir_label(generator, "increment") : ir_copy_string(test);
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

bool generate_ir(Node *program, FILE *output) {
    IRGenerator *generator = (IRGenerator *)calloc(1, sizeof(IRGenerator));
    if (generator == NULL) {
        fprintf(stderr, "Erro: memória insuficiente para inicializar a geração de IR.\n");
        return false;
    }
    ir_output = output != NULL ? output : stdout;
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
                fprintf(ir_output, "global %s = %s\n", node->b, value);
                free(value);
            }
        } else if (node->kind == N_FUNCTION) {
            generator->next_temp = 0;
            generator->next_label = 0;
            generator->next_shadow = 0;
            generator->loop_count = 0;
            ir_push_scope(generator);
            fprintf(ir_output, "func %s", node->b);
            if (node->params != NULL) {
                fputc('(', ir_output);
                for (Param *param = node->params; param != NULL; param = param->next) {
                    if (param != node->params) fprintf(ir_output, ", ");
                    fprintf(ir_output, "%s", param->name);
                    char *emitted = ir_declare(generator, param->name);
                    free(emitted);
                }
                fputc(')', ir_output);
            }
            fputc('\n', ir_output);
            ir_block(generator, node->left, false);
            fprintf(ir_output, "endfunc\n");
            ir_pop_scope(generator);
        }
    }
    ir_output = NULL;
    free(generator);
    return true;
}

