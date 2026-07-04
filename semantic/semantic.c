#include "semantic.h"
#include "symbol_table.h"

/*
 For scopes, implementation of symantic table tree
                    Global
                   /    \
             ___ scope1  scope2
            /    /    \
        scope4 scope5 scope6
each scope will be a seprate symantic table, and the scopes can access there
parents
example being

x = 10 // global
{
    y = x + 10; // allowed
    {
        a = y + x; // allowed
    }
}
z = y; // not allowed
*/

void semantic_analyze(ASTNode *root) {
  SymbolTable *table = st_create(128);

  analyze_node(root, table);

  st_free(table);
}

void create_newscope(ASTNode *node, SymbolTable *parent) {
  SymbolTable *child = st_create(128);

  child->parent = parent; // child can climb toward outer scopes

  for (int i = 0; i < node->child_count; i++) {
    analyze_node(node->children[i], child);
  }

  st_free(child);
}

void analyze_node(ASTNode *node, SymbolTable *table) {

  if (node == NULL)
    return;

  switch (node->type) {
  case NODE_PROGRAM:
    for (int i = 0; i < node->child_count; i++) {
      analyze_node(node->children[i], table);
    }
    break;

  case NODE_BLOCK: // make changes so it creates a new symboltable and points to
                   // previous one
    create_newscope(node, table); // In the case of new block the code will come
                                  // here and a new scope is created
    break;

  case NODE_ASSIGN:
    analyze_node(node->right, table);
    symbol_insert(table, node->left->token.literal);
    break;

  case NODE_IDENT:
    if (!symbol_exists(table, node->token.literal)) {
      printf("Semantic Error: undefined variable '%s'\n", node->token.literal);
    }
    break;
  case NODE_ADD:
  case NODE_SUB:
  case NODE_MUL:
  case NODE_DIV:
  case NODE_MOD:
  case NODE_EQ:
  case NODE_NEQ:
  case NODE_GT:
  case NODE_LT:
  case NODE_GTE:
  case NODE_LTE:
    analyze_node(node->left, table);
    analyze_node(node->right, table);
    break;

  case NODE_NEG:
  case NODE_NOT:
    analyze_node(node->left, table);
    break;

  case NODE_CALL:
    for (int i = 0; i < node->child_count; i++) {
      analyze_node(node->children[i], table);
    }
    break;

  case NODE_IF:
  case NODE_ELIF:
    analyze_node(node->left, table);
    analyze_node(node->right, table);
    for (int i = 0; i < node->child_count; i++) {
      analyze_node(node->children[i], table);
    }
    break;

  case NODE_ELSE:
    analyze_node(node->right, table);
    break;

  case NODE_REPEAT:
    analyze_node(node->left, table);
    analyze_node(node->right, table);
    break;

  case NODE_PROBE:
    analyze_node(node->right, table);
    break;

  case NODE_NUMBER:
  case NODE_STRING:
    return;
  default:
    break;
  }
}
