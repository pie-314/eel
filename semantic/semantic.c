#include "semantic.h"
#include "symbol_table.h"
#include <string.h>

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

DataType check_expression(ASTNode *node, SymbolTable *table, SemanticContext *ctx) {
  if (node == NULL)
    return TYPE_UNKNOWN;

  switch (node->type) {
  case NODE_NUMBER:
    return TYPE_INT;

  case NODE_STRING:
    return TYPE_STR;

  case NODE_IDENT: {
    Symbol *sym = symbol_lookup(table, node->token.literal);
    if (sym == NULL) {
      if (strcmp(node->token.literal, "pid") == 0 ||
          strcmp(node->token.literal, "uid") == 0 ||
          strcmp(node->token.literal, "gid") == 0) {
        Symbol *builtin = symbol_insert(table, node->token.literal, TYPE_INT, 0);
        builtin->is_builtin = true;
        return TYPE_INT;
      }
      ctx->errors++;
      printf("Semantic Error: undefined variable '%s'\n", node->token.literal);
      return TYPE_UNKNOWN;
    }
    return sym->type;
  }

  case NODE_ADD:
  case NODE_SUB:
  case NODE_MUL:
  case NODE_DIV:
  case NODE_MOD: {
    DataType left_type = check_expression(node->left, table, ctx);
    DataType right_type = check_expression(node->right, table, ctx);

    if (left_type != TYPE_UNKNOWN && right_type != TYPE_UNKNOWN) {
      if (left_type != TYPE_INT || right_type != TYPE_INT) {
        ctx->errors++;
        printf("Semantic Error: operator '%s' requires integer operands\n", node->token.literal);
      }
    }
    return TYPE_INT;
  }

  case NODE_EQ:
  case NODE_NEQ:
  case NODE_GT:
  case NODE_LT:
  case NODE_GTE:
  case NODE_LTE: {
    DataType left_type = check_expression(node->left, table, ctx);
    DataType right_type = check_expression(node->right, table, ctx);

    if (left_type != TYPE_UNKNOWN && right_type != TYPE_UNKNOWN) {
      if (left_type != right_type) {
        ctx->errors++;
        printf("Semantic Error: type mismatch in comparison '%s'\n", node->token.literal);
      }
    }
    return TYPE_INT;
  }

  case NODE_NEG:
  case NODE_NOT: {
    DataType operand_type = check_expression(node->left, table, ctx);
    if (operand_type != TYPE_UNKNOWN && operand_type != TYPE_INT) {
      ctx->errors++;
      printf("Semantic Error: unary operator requires integer operand\n");
    }
    return TYPE_INT;
  }

  case NODE_CALL: {
    const char *fn = node->left->token.literal;
    if (strcmp(fn, "print") == 0) {
      if (node->child_count > 4) {
        ctx->errors++;
        printf("Semantic Error: print() exceeds argument limit (max 3 values)\n");
      }
      for (int i = 0; i < node->child_count; i++) {
        check_expression(node->children[i], table, ctx);
      }
      return TYPE_VOID;
    } else if (strcmp(fn, "ktime") == 0) {
      return TYPE_INT;
    } else {
      ctx->errors++;
      printf("Semantic Error: unknown function '%s'\n", fn);
      return TYPE_UNKNOWN;
    }
  }

  default:
    return TYPE_UNKNOWN;
  }
}

bool semantic_analyze(ASTNode *root) {
  SemanticContext ctx = { .errors = 0, .current_stack_offset = 0 };
  SymbolTable *table = st_create(128);

  analyze_node(root, table, &ctx);

  st_free(table);
  return ctx.errors == 0;
}

void create_newscope(ASTNode *node, SymbolTable *parent, SemanticContext *ctx) {
  SymbolTable *child = st_create(128);

  child->parent = parent; // child can climb toward outer scopes

  for (int i = 0; i < node->child_count; i++) {
    analyze_node(node->children[i], child, ctx);
  }

  st_free(child);
}

void analyze_node(ASTNode *node, SymbolTable *table, SemanticContext *ctx) {

  if (node == NULL)
    return;

  switch (node->type) {
  case NODE_PROGRAM:
    for (int i = 0; i < node->child_count; i++) {
      analyze_node(node->children[i], table, ctx);
    }
    break;

  case NODE_BLOCK: // make changes so it creates a new symboltable and points to
                   // previous one
    create_newscope(node, table, ctx); // In the case of new block the code will come
                                       // here and a new scope is created
    break;

  case NODE_ASSIGN: {
    DataType rhs_type = check_expression(node->right, table, ctx);
    const char *name = node->left->token.literal;

    Symbol *sym = symbol_lookup(table, name);
    if (sym == NULL) {
      ctx->current_stack_offset += 8;
      symbol_insert(table, name, rhs_type != TYPE_UNKNOWN ? rhs_type : TYPE_INT, -ctx->current_stack_offset);
    } else {
      if (rhs_type != TYPE_UNKNOWN && sym->type != TYPE_UNKNOWN && sym->type != rhs_type) {
        ctx->errors++;
        printf("Semantic Error: type mismatch in assignment to '%s'\n", name);
      }
    }
    break;
  }

  case NODE_IDENT:
    if (!symbol_exists(table, node->token.literal)) {
      ctx->errors++;
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
  case NODE_NEG:
  case NODE_NOT:
  case NODE_CALL:
  case NODE_NUMBER:
  case NODE_STRING:
  case NODE_EXPR_STMT:
    check_expression(node, table, ctx);
    break;

  case NODE_IF:
    check_expression(node->left, table, ctx);
    analyze_node(node->right, table, ctx);
    for (int i = 0; i < node->child_count; i++) {
      analyze_node(node->children[i], table, ctx);
    }
    break;

  case NODE_ELIF:
    check_expression(node->left, table, ctx);
    analyze_node(node->right, table, ctx);
    break;

  case NODE_ELSE:
    analyze_node(node->right, table, ctx);
    break;

  case NODE_REPEAT:
    if (node->left != NULL) {
      if (node->left->type == NODE_NUMBER) {
        long count = atol(node->left->token.literal);
        if (count <= 0) {
          ctx->errors++;
          printf("Semantic Error: repeat count must be greater than 0\n");
        } else if (count > 256) {
          ctx->errors++;
          printf("Semantic Error: repeat count (%ld) exceeds verifier limit (max 256)\n", count);
        }
      } else if (node->left->type == NODE_IDENT) {
        Symbol *sym = symbol_lookup(table, node->left->token.literal);
        if (sym == NULL) {
          ctx->errors++;
          printf("Semantic Error: undefined variable '%s'\n", node->left->token.literal);
        } else if (sym->type != TYPE_INT) {
          ctx->errors++;
          printf("Semantic Error: repeat count must be an integer\n");
        }
      } else {
        check_expression(node->left, table, ctx);
      }
    }
    analyze_node(node->right, table, ctx);
    break;

  case NODE_PROBE: {
    ctx->current_stack_offset = 0;

    Symbol *builtin_pid = symbol_insert(table, "pid", TYPE_INT, 0);
    builtin_pid->is_builtin = true;

    analyze_node(node->right, table, ctx);

    if (ctx->current_stack_offset > 512) {
      ctx->errors++;
      printf("Semantic Error: probe exceeds eBPF 512-byte stack limit (allocated %d bytes)\n",
             ctx->current_stack_offset);
    }
    break;
  }

  default:
    break;
  }
}
