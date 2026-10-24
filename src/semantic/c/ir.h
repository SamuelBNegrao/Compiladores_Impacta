#ifndef MINIC_IR_H
#define MINIC_IR_H

#include "../../lexer/parser.h"

#include <stdbool.h>
#include <stdio.h>

bool generate_ir(Node *program, FILE *output);

#endif
