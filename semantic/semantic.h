#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "../parser/parser.h"
#include "symbol_table.h"

void semantic_analyze(ASTNode *root);

void analyze_node(ASTNode *node, SymbolTable *table);

void create_newscope(ASTNode *node, SymbolTable *parent);

#endif
