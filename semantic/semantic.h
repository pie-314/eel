#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../parser/parser.h"
#include "symbol_table.h"
#include <stdbool.h>

typedef struct {
  int errors;
  int current_stack_offset;
} SemanticContext;

bool semantic_analyze(ASTNode *root);
void analyze_node(ASTNode *node, SymbolTable *table, SemanticContext *ctx);
void create_newscope(ASTNode *node, SymbolTable *parent, SemanticContext *ctx);
DataType check_expression(ASTNode *node, SymbolTable *table, SemanticContext *ctx);

#endif
