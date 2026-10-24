#include "optimizer.h"

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *optimizer_copy_string(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = (char *)malloc(length);
    if (copy != NULL) memcpy(copy, text, length);
    return copy;
}

static char *optimizer_format_string(const char *format, ...) {
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
        return NULL;
    }
    text = (char *)malloc((size_t)length + 1);
    if (text != NULL) vsnprintf(text, (size_t)length + 1, format, args);
    va_end(args);
    return text;
}

typedef struct {
    char **items;
    int count;
    int capacity;
} IRText;

static bool ir_text_append(IRText *text, const char *line) {
    if (text->count == text->capacity) {
        int capacity = text->capacity == 0 ? 32 : text->capacity * 2;
        char **items = (char **)realloc(text->items, (size_t)capacity * sizeof(char *));
        if (items == NULL) return false;
        text->items = items;
        text->capacity = capacity;
    }
    text->items[text->count] = optimizer_copy_string(line);
    if (text->items[text->count] == NULL) return false;
    text->count++;
    return true;
}

static void ir_text_free(IRText *text) {
    for (int index = 0; index < text->count; index++) free(text->items[index]);
    free(text->items);
    memset(text, 0, sizeof(*text));
}

static char *ir_trim(char *line) {
    while (*line == ' ' || *line == '\t' || *line == '\r') line++;
    size_t length = strlen(line);
    while (length > 0 && (line[length - 1] == ' ' || line[length - 1] == '\t' ||
                          line[length - 1] == '\r')) line[--length] = '\0';
    return line;
}

static bool ir_assignment(const char *line, char *target, size_t target_size,
                          char *expression, size_t expression_size) {
    const char *separator = strstr(line, " = ");
    if (separator == NULL) return false;
    size_t target_length = (size_t)(separator - line);
    if (target_length == 0 || target_length >= target_size) return false;
    memcpy(target, line, target_length);
    target[target_length] = '\0';
    snprintf(expression, expression_size, "%s", separator + 3);
    return true;
}

static bool ir_is_number(const char *text, double *value, bool *integer) {
    char *end = NULL;
    if (text == NULL || *text == '\0') return false;
    double parsed = strtod(text, &end);
    if (end == text || *end != '\0') return false;
    if (value != NULL) *value = parsed;
    if (integer != NULL) *integer = strchr(text, '.') == NULL;
    return true;
}

static bool ir_is_literal(const char *text) {
    double ignored;
    if (ir_is_number(text, &ignored, NULL)) return true;
    return strcmp(text, "true") == 0 || strcmp(text, "false") == 0 ||
           (text[0] == '\'' && text[strlen(text) > 0 ? strlen(text) - 1 : 0] == '\'') ||
           (text[0] == '"' && text[strlen(text) > 0 ? strlen(text) - 1 : 0] == '"');
}

static bool ir_identifier_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '#';
}

static bool ir_contains_name(const char *text, const char *name) {
    size_t length = strlen(name);
    const char *cursor = text;
    while ((cursor = strstr(cursor, name)) != NULL) {
        bool left = cursor == text || !ir_identifier_char(cursor[-1]);
        bool right = !ir_identifier_char(cursor[length]);
        if (left && right) return true;
        cursor += length;
    }
    return false;
}

static bool ir_is_label(const char *line) {
    size_t length = strlen(line);
    return length > 1 && line[length - 1] == ':';
}

static bool ir_target_of(const char *line, char *target, size_t target_size) {
    const char *text = NULL;
    if (strncmp(line, "goto ", 5) == 0) text = line + 5;
    else if (strncmp(line, "ifFalse ", 8) == 0) {
        const char *goto_part = strstr(line + 8, " goto ");
        if (goto_part != NULL) text = goto_part + 6;
    }
    if (text == NULL || *text == '\0') return false;
    snprintf(target, target_size, "%s", text);
    return true;
}

static void ir_substitute(char *operand, char names[][256], char values[][512],
                          int count) {
    for (int depth = 0; depth < count; depth++) {
        bool changed = false;
        for (int index = count - 1; index >= 0; index--) {
            if (strcmp(operand, names[index]) == 0 &&
                strcmp(operand, values[index]) != 0) {
                snprintf(operand, 256, "%.255s", values[index]);
                changed = true;
                break;
            }
        }
        if (!changed) break;
    }
}

static bool ir_fold_binary(const char *left, const char *op, const char *right,
                           char *result, size_t result_size) {
    double a;
    double b;
    bool integer_a;
    bool integer_b;
    if ((strcmp(op, "and") == 0 || strcmp(op, "or") == 0) &&
        (strcmp(left, "true") == 0 || strcmp(left, "false") == 0) &&
        (strcmp(right, "true") == 0 || strcmp(right, "false") == 0)) {
        bool value = strcmp(op, "and") == 0
            ? strcmp(left, "true") == 0 && strcmp(right, "true") == 0
            : strcmp(left, "true") == 0 || strcmp(right, "true") == 0;
        snprintf(result, result_size, "%s", value ? "true" : "false");
        return true;
    }
    if (!ir_is_number(left, &a, &integer_a) || !ir_is_number(right, &b, &integer_b))
        return false;
    bool floating = !integer_a || !integer_b;
    double value;
    if (strcmp(op, "+") == 0) value = a + b;
    else if (strcmp(op, "-") == 0) value = a - b;
    else if (strcmp(op, "*") == 0) value = a * b;
    else if (strcmp(op, "/") == 0) {
        if (b == 0.0) return false;
        value = floating ? a / b : (double)((long long)a / (long long)b);
    } else if (strcmp(op, "%") == 0) {
        if (b == 0.0 || !integer_a || !integer_b) return false;
        value = (double)((long long)a % (long long)b);
    } else if (strcmp(op, "==") == 0) {
        snprintf(result, result_size, "%s", a == b ? "true" : "false");
        return true;
    } else if (strcmp(op, "!=") == 0) {
        snprintf(result, result_size, "%s", a != b ? "true" : "false");
        return true;
    } else if (strcmp(op, "<") == 0) {
        snprintf(result, result_size, "%s", a < b ? "true" : "false");
        return true;
    } else if (strcmp(op, ">") == 0) {
        snprintf(result, result_size, "%s", a > b ? "true" : "false");
        return true;
    } else if (strcmp(op, "<=") == 0) {
        snprintf(result, result_size, "%s", a <= b ? "true" : "false");
        return true;
    } else if (strcmp(op, ">=") == 0) {
        snprintf(result, result_size, "%s", a >= b ? "true" : "false");
        return true;
    } else return false;
    if (!floating && value >= (double)LLONG_MIN && value <= (double)LLONG_MAX)
        snprintf(result, result_size, "%.0f", value);
    else
        snprintf(result, result_size, "%.15g", value);
    return true;
}

static bool ir_simplify_binary(char *left, const char *op, char *right,
                               char *result, size_t result_size) {
    if (ir_fold_binary(left, op, right, result, result_size)) return true;
    if (strcmp(op, "+") == 0 && strcmp(right, "0") == 0) {
        snprintf(result, result_size, "%s", left); return true;
    }
    if (strcmp(op, "+") == 0 && strcmp(left, "0") == 0) {
        snprintf(result, result_size, "%s", right); return true;
    }
    if (strcmp(op, "-") == 0 && strcmp(right, "0") == 0) {
        snprintf(result, result_size, "%s", left); return true;
    }
    if (strcmp(op, "*") == 0 && strcmp(right, "1") == 0) {
        snprintf(result, result_size, "%s", left); return true;
    }
    if (strcmp(op, "*") == 0 && strcmp(left, "1") == 0) {
        snprintf(result, result_size, "%s", right); return true;
    }
    if (strcmp(op, "*") == 0 && (strcmp(left, "0") == 0 || strcmp(right, "0") == 0)) {
        snprintf(result, result_size, "0"); return true;
    }
    if (strcmp(op, "/") == 0 && strcmp(right, "1") == 0) {
        snprintf(result, result_size, "%s", left); return true;
    }
    return false;
}

static void ir_propagate(IRText *body) {
    char names[256][256];
    char values[256][512];
    int mappings = 0;
    char common_expression[256][512];
    char common_value[256][256];
    int common_count = 0;
    IRText output = {0};
    for (int index = 0; index < body->count; index++) {
        char *line = body->items[index];
        if (ir_is_label(line) || strncmp(line, "goto ", 5) == 0) {
            mappings = 0;
            common_count = 0;
        }
        if (ir_is_label(line)) {
            ir_text_append(&output, line);
            continue;
        }
        if (strncmp(line, "ifFalse ", 8) == 0) {
            char condition[256], target[256];
            const char *goto_part = strstr(line + 8, " goto ");
            if (goto_part != NULL) {
                size_t length = (size_t)(goto_part - (line + 8));
                if (length < sizeof(condition)) {
                    memcpy(condition, line + 8, length);
                    condition[length] = '\0';
                    snprintf(target, sizeof(target), "%s", goto_part + 6);
                    ir_substitute(condition, names, values, mappings);
                    if (strcmp(condition, "true") == 0) continue;
                    if (strcmp(condition, "false") == 0) {
                        ir_text_append(&output, optimizer_format_string("goto %s", target));
                        continue;
                    }
                    ir_text_append(&output, optimizer_format_string("ifFalse %s goto %s", condition, target));
                } else ir_text_append(&output, line);
            } else ir_text_append(&output, line);
            mappings = 0;
            common_count = 0;
            continue;
        }
        char target[256], expression[512];
        if (ir_assignment(line, target, sizeof(target), expression, sizeof(expression))) {
            if (strncmp(expression, "call ", 5) == 0) {
                mappings = 0;
                common_count = 0;
            }
            char left[256], op[32], right[256];
            if (sscanf(expression, "%31s %255s", op, left) == 2 &&
                strlen(op) + strlen(left) + 1 == strlen(expression) &&
                (strcmp(op, "neg") == 0 || strcmp(op, "not") == 0)) {
                ir_substitute(left, names, values, mappings);
                double number;
                if (strcmp(op, "neg") == 0 && ir_is_number(left, &number, NULL)) {
                    snprintf(expression, sizeof(expression), "%.15g", -number);
                } else if (strcmp(op, "not") == 0 &&
                           (strcmp(left, "true") == 0 || strcmp(left, "false") == 0)) {
                    snprintf(expression, sizeof(expression), "%s",
                             strcmp(left, "true") == 0 ? "false" : "true");
                } else {
                    snprintf(expression, sizeof(expression), "%s %.255s", op, left);
                }
            } else if (sscanf(expression, "%255s %31s %255s", left, op, right) == 3 &&
                strlen(left) + strlen(op) + strlen(right) + 2 == strlen(expression)) {
                ir_substitute(left, names, values, mappings);
                ir_substitute(right, names, values, mappings);
                char folded[512];
                if (ir_simplify_binary(left, op, right, folded, sizeof(folded))) {
                    snprintf(expression, sizeof(expression), "%s", folded);
                } else {
                    snprintf(expression, sizeof(expression), "%.230s %.30s %.230s", left, op, right);
                    bool found = false;
                    for (int common = common_count - 1; common >= 0; common--) {
                        if (strcmp(common_expression[common], expression) == 0) {
                            snprintf(expression, sizeof(expression), "%s", common_value[common]);
                            found = true;
                            break;
                        }
                    }
                    if (!found && common_count < 256) {
                        snprintf(common_expression[common_count], sizeof(common_expression[common_count]), "%s", expression);
                        snprintf(common_value[common_count], sizeof(common_value[common_count]), "%s", target);
                        common_count++;
                    }
                }
            } else if (ir_is_literal(expression) || strchr(expression, '#') != NULL ||
                       (expression[0] != '\0' &&
                        ((expression[0] >= 'a' && expression[0] <= 'z') ||
                         (expression[0] >= 'A' && expression[0] <= 'Z') ||
                         expression[0] == '_'))) {
                ir_substitute(expression, names, values, mappings);
            }
            for (int map = 0; map < mappings;) {
                if (strcmp(names[map], target) == 0 ||
                    ir_contains_name(values[map], target)) {
                    mappings--;
                    if (map != mappings) {
                        memcpy(names[map], names[mappings], sizeof(names[map]));
                        memcpy(values[map], values[mappings], sizeof(values[map]));
                    }
                } else map++;
            }
            for (int common = 0; common < common_count;) {
                if (ir_contains_name(common_expression[common], target)) {
                    common_count--;
                    if (common != common_count) {
                        memcpy(common_expression[common], common_expression[common_count],
                               sizeof(common_expression[common]));
                        memcpy(common_value[common], common_value[common_count],
                               sizeof(common_value[common]));
                    }
                } else common++;
            }
            if (strcmp(expression, target) != 0 && mappings < 256 &&
                (ir_is_literal(expression) ||
                 (expression[0] != '\0' && strchr(expression, ' ') == NULL &&
                  !strstr(expression, "call") && !strstr(expression, "load")))) {
                snprintf(names[mappings], sizeof(names[mappings]), "%s", target);
                snprintf(values[mappings], sizeof(values[mappings]), "%.511s", expression);
                mappings++;
            }
            ir_text_append(&output, optimizer_format_string("%s = %s", target, expression));
            continue;
        }
        if (strncmp(line, "print ", 6) == 0 || strncmp(line, "return ", 7) == 0) {
            const char *prefix = line[0] == 'p' ? "print " : "return ";
            char value[512];
            snprintf(value, sizeof(value), "%s", line + strlen(prefix));
            ir_substitute(value, names, values, mappings);
            ir_text_append(&output, optimizer_format_string("%s%s", prefix, value));
            if (line[0] == 'r') { mappings = 0; common_count = 0; }
            continue;
        }
        if (strncmp(line, "param ", 6) == 0 || strncmp(line, "read ", 5) == 0 ||
            strncmp(line, "store ", 6) == 0) {
            mappings = 0;
            common_count = 0;
        }
        ir_text_append(&output, line);
    }
    ir_text_free(body);
    *body = output;
}

static int ir_reference_count(const IRText *body, const char *label) {
    int count = 0;
    for (int index = 0; index < body->count; index++) {
        char target[256];
        if (ir_target_of(body->items[index], target, sizeof(target)) &&
            strcmp(target, label) == 0) count++;
    }
    return count;
}

static void ir_remove_unreachable(IRText *body) {
    IRText output = {0};
    bool unreachable = false;
    for (int index = 0; index < body->count; index++) {
        char *line = body->items[index];
        if (ir_is_label(line)) {
            char label[256];
            size_t length = strlen(line) - 1;
            if (length >= sizeof(label)) continue;
            memcpy(label, line, length);
            label[length] = '\0';
            if (unreachable && ir_reference_count(body, label) == 0) continue;
            unreachable = false;
            ir_text_append(&output, line);
        } else if (!unreachable) {
            ir_text_append(&output, line);
            if (strncmp(line, "return", 6) == 0 || strncmp(line, "goto ", 5) == 0)
                unreachable = true;
        }
    }
    ir_text_free(body);
    *body = output;
}

static void ir_remove_redundant_jumps(IRText *body) {
    IRText output = {0};
    for (int index = 0; index < body->count; index++) {
        if (strncmp(body->items[index], "goto ", 5) == 0 && index + 1 < body->count &&
            ir_is_label(body->items[index + 1])) {
            char target[256];
            snprintf(target, sizeof(target), "%s", body->items[index] + 5);
            size_t length = strlen(body->items[index + 1]) - 1;
            if (strlen(target) == length &&
                strncmp(body->items[index + 1], target, length) == 0 &&
                ir_reference_count(body, target) == 1) {
                index++;
                continue;
            }
        }
        ir_text_append(&output, body->items[index]);
    }
    ir_text_free(body);
    *body = output;
}

static bool ir_pure_assignment(const char *expression) {
    return strstr(expression, "call ") == NULL && strncmp(expression, "load ", 5) != 0;
}

static void ir_remove_dead_assignments(IRText *body) {
    bool changed = true;
    while (changed) {
        changed = false;
        IRText output = {0};
        for (int index = 0; index < body->count; index++) {
            char target[256], expression[512];
            bool remove = false;
            if (ir_assignment(body->items[index], target, sizeof(target),
                              expression, sizeof(expression)) &&
                ir_pure_assignment(expression)) {
                bool used = false;
                for (int other = 0; other < body->count; other++) {
                    if (other == index) continue;
                    char other_target[256], other_expression[512];
                    const char *text = body->items[other];
                    if (ir_assignment(text, other_target, sizeof(other_target),
                                      other_expression, sizeof(other_expression))) {
                        if (ir_contains_name(other_expression, target)) used = true;
                    } else if (strncmp(text, "ifFalse ", 8) == 0 ||
                               strncmp(text, "print ", 6) == 0 ||
                               strncmp(text, "return ", 7) == 0 ||
                               strncmp(text, "param ", 6) == 0 ||
                               strncmp(text, "read ", 5) == 0 ||
                               strncmp(text, "store ", 6) == 0) {
                        if (ir_contains_name(text, target)) used = true;
                    }
                }
                remove = !used;
            }
            if (remove) changed = true;
            else ir_text_append(&output, body->items[index]);
        }
        ir_text_free(body);
        *body = output;
    }
}

static void ir_move_loop_invariants(IRText *body) {
    for (int label_index = 0; label_index < body->count; label_index++) {
        if (!ir_is_label(body->items[label_index])) continue;
        char header[256];
        size_t label_length = strlen(body->items[label_index]) - 1;
        if (label_length >= sizeof(header)) continue;
        memcpy(header, body->items[label_index], label_length);
        header[label_length] = '\0';
        int back_edge = -1;
        for (int index = label_index + 1; index < body->count; index++) {
            char target[256];
            if (ir_target_of(body->items[index], target, sizeof(target)) &&
                strcmp(target, header) == 0 && strncmp(body->items[index], "goto ", 5) == 0) {
                back_edge = index;
            }
        }
        if (back_edge < 0) continue;
        char writes[256][256];
        int write_count = 0;
        bool safe = true;
        for (int index = label_index + 1; index < back_edge; index++) {
            char target[256], expression[512];
            if (ir_assignment(body->items[index], target, sizeof(target),
                              expression, sizeof(expression))) {
                if (write_count < 256) snprintf(writes[write_count++], sizeof(writes[0]), "%s", target);
            } else if (strncmp(body->items[index], "read ", 5) == 0 ||
                       strncmp(body->items[index], "store ", 6) == 0 ||
                       strncmp(body->items[index], "param ", 6) == 0) safe = false;
        }
        if (!safe) continue;
        char *moves[64];
        int move_count = 0;
        bool moved[4096] = {false};
        if (back_edge >= (int)(sizeof(moved) / sizeof(moved[0]))) continue;
        for (int index = label_index + 1; index < back_edge && move_count < 64; index++) {
            char target[256], expression[512], left[256], op[32], right[256];
            if (!ir_assignment(body->items[index], target, sizeof(target),
                               expression, sizeof(expression)) ||
                target[0] != 't' || target[1] < '0' || target[1] > '9' ||
                sscanf(expression, "%255s %31s %255s", left, op, right) != 3 ||
                strlen(left) + strlen(op) + strlen(right) + 2 != strlen(expression) ||
                strcmp(op, "/") == 0 || strcmp(op, "%") == 0) continue;
            bool invariant = true;
            for (int write = 0; write < write_count; write++) {
                if (ir_contains_name(expression, writes[write])) invariant = false;
            }
            if (invariant) {
                moves[move_count++] = optimizer_copy_string(body->items[index]);
                moved[index] = true;
            }
        }
        if (move_count == 0) continue;
        IRText output = {0};
        for (int index = 0; index < body->count; index++) {
            if (index == label_index) {
                for (int move = 0; move < move_count; move++) ir_text_append(&output, moves[move]);
            }
            if (index < (int)(sizeof(moved) / sizeof(moved[0])) && moved[index]) continue;
            ir_text_append(&output, body->items[index]);
        }
        for (int move = 0; move < move_count; move++) free(moves[move]);
        ir_text_free(body);
        *body = output;
        label_index += move_count;
    }
}

bool optimize_ir_text(const char *input, FILE *output) {
    IRText lines = {0};
    char *copy = optimizer_copy_string(input);
    if (copy == NULL) return false;
    char *line = copy;
    while (*line != '\0') {
        char *next = strchr(line, '\n');
        if (next != NULL) *next = '\0';
        if (!ir_text_append(&lines, ir_trim(line))) {
            free(copy);
            ir_text_free(&lines);
            return false;
        }
        if (next == NULL) break;
        line = next + 1;
    }
    free(copy);
    for (int index = 0; index < lines.count;) {
        if (strncmp(lines.items[index], "func ", 5) != 0) {
            fprintf(output, "%s\n", lines.items[index++]);
            continue;
        }
        int end = index + 1;
        while (end < lines.count && strcmp(lines.items[end], "endfunc") != 0) end++;
        IRText body = {0};
        for (int item = index + 1; item < end; item++) ir_text_append(&body, lines.items[item]);
        ir_move_loop_invariants(&body);
        for (int pass = 0; pass < 8; pass++) {
            int before = body.count;
            ir_remove_unreachable(&body);
            ir_remove_redundant_jumps(&body);
            ir_propagate(&body);
            ir_remove_unreachable(&body);
            ir_remove_redundant_jumps(&body);
            ir_remove_dead_assignments(&body);
            if (body.count == before) break;
        }
        fprintf(output, "%s\n", lines.items[index]);
        for (int item = 0; item < body.count; item++) {
            if (ir_is_label(body.items[item])) fprintf(output, "%s\n", body.items[item]);
            else fprintf(output, "  %s\n", body.items[item]);
        }
        if (end < lines.count) fprintf(output, "endfunc\n");
        ir_text_free(&body);
        index = end < lines.count ? end + 1 : end;
    }
    ir_text_free(&lines);
    return !ferror(output);
}

